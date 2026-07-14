#!/usr/bin/env python3
"""Minimal LSP-protocol regression test for `wyn lsp`.

Speaks LSP over stdio to the compiler's built-in language server and asserts the
core editor features work end to end — the same surface the nvim-wyn / vscode-wyn
plugins depend on:

  - initialize returns the advertised capabilities (NOT null — a whitespace bug
    in method parsing once made every request answer null)
  - diagnostics come from `wyn check` (type-check only, no execution) and carry
    the real error MESSAGE, not a bare "--> file:line" location
  - hover / completion / definition respond sensibly

Runs against ./wyn (override with $WYN). No third-party deps. Exit 0 = PASS.
"""
import json, os, subprocess, sys, time, threading, tempfile

WYN = os.environ.get("WYN", "./wyn")


class Client:
    def __init__(self):
        self.p = subprocess.Popen([WYN, "lsp"], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.responses = {}   # id -> result
        self.diagnostics = {} # uri -> latest diagnostics list
        self._lock = threading.Lock()
        # Read LSP frames on a background thread. We use a blocking reader rather
        # than select(): on Windows select() only works on sockets, not pipes
        # (OSError WinError 10093), so a thread is the portable choice.
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self):
        out = self.p.stdout
        while True:
            header = b""
            while b"\r\n\r\n" not in header:
                c = out.read(1)
                if not c:
                    return  # pipe closed
                header += c
            if b"Content-Length:" not in header:
                continue
            n = int(header.split(b"Content-Length:")[1].split(b"\r\n")[0])
            body = out.read(n)
            try:
                m = json.loads(body)
            except Exception:
                continue
            with self._lock:
                if m.get("id") is not None and "result" in m:
                    self.responses[str(m["id"])] = m["result"]
                if m.get("method") == "textDocument/publishDiagnostics":
                    self.diagnostics[m["params"]["uri"]] = m["params"]["diagnostics"]

    def send(self, obj, compact=True):
        sep = (",", ":") if compact else (", ", ": ")
        data = json.dumps(obj, separators=sep)
        self.p.stdin.write(f"Content-Length: {len(data)}\r\n\r\n{data}".encode())
        self.p.stdin.flush()

    def pump(self, seconds):
        # Messages arrive on the reader thread; just wait for them to land.
        time.sleep(seconds)

    def get_response(self, rid):
        with self._lock:
            return self.responses.get(str(rid))

    def get_diagnostics(self, uri):
        with self._lock:
            return self.diagnostics.get(uri)

    def stop(self):
        try:
            self.send({"jsonrpc": "2.0", "id": 999, "method": "shutdown", "params": {}})
            time.sleep(0.2)
        finally:
            self.p.terminate()


FAILS = []


def check(cond, label):
    print(("  ok  " if cond else " FAIL ") + label)
    if not cond:
        FAILS.append(label)


def main():
    workdir = tempfile.mkdtemp(prefix="wyn_lsp_test_")
    uri = "file://" + os.path.join(workdir, "prog.wyn")
    c = Client()

    # 1. initialize — PRETTY-printed JSON on purpose (regression: the server used
    #    to only match `"method":"x"` with no space and answered null for these).
    c.send({"jsonrpc": "2.0", "id": 1, "method": "initialize",
            "params": {"capabilities": {}}}, compact=False)
    c.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
    c.pump(1.0)
    init = c.get_response(1)
    check(isinstance(init, dict) and "capabilities" in init,
          "initialize returns capabilities (pretty JSON parses)")
    caps = (init or {}).get("capabilities", {})
    for cap in ("hoverProvider", "completionProvider", "definitionProvider",
                "referencesProvider", "renameProvider"):
        check(cap in caps, f"capability advertised: {cap}")

    # 2. didOpen a program with ONE type error → diagnostic with the real message.
    src = ('fn helper(n: int) -> int {\n'
           '    return n * 2\n'
           '}\n'
           '\n'
           'fn main() -> int {\n'
           '    var x: int = "not an int"\n'   # line 5: type mismatch
           '    return helper(x)\n'
           '}\n')
    c.send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "wyn",
                                        "version": 1, "text": src}}})
    c.pump(3.0)
    diags = c.get_diagnostics(uri) or []
    check(len(diags) >= 1, "type error produces a diagnostic")
    if diags:
        d = diags[0]
        check(d.get("line", d["range"]["start"]["line"]) == 5 or
              d["range"]["start"]["line"] == 5, "diagnostic on the offending line (5)")
        msg = d.get("message", "")
        check(msg and "-->" not in msg,
              f"diagnostic carries a real message, not a location: {msg!r}")
        check(d.get("source") == "wyn", "diagnostic source is 'wyn'")

    # 3. a CLEAN program clears diagnostics (proves check ran, found nothing).
    good_uri = "file://" + os.path.join(workdir, "ok.wyn")
    c.send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": good_uri, "languageId": "wyn",
                                        "version": 1,
                                        "text": "fn main() -> int {\n    return 0\n}\n"}}})
    c.pump(2.5)
    check(len(c.get_diagnostics(good_uri) or []) == 0, "clean program has no diagnostics")

    # 4. hover over the `helper` call → returns markdown contents.
    c.send({"jsonrpc": "2.0", "id": 2, "method": "textDocument/hover",
            "params": {"textDocument": {"uri": uri},
                       "position": {"line": 6, "character": 12}}})
    c.pump(1.5)
    hov = c.get_response(2)
    check(isinstance(hov, dict) and "contents" in hov, "hover returns contents")

    # 5. completion → a non-empty list of items.
    c.send({"jsonrpc": "2.0", "id": 3, "method": "textDocument/completion",
            "params": {"textDocument": {"uri": uri},
                       "position": {"line": 6, "character": 12},
                       "context": {"triggerCharacter": "."}}})
    c.pump(1.5)
    comp = c.get_response(3)
    items = comp.get("items", []) if isinstance(comp, dict) else comp
    check(isinstance(items, list) and len(items) > 0, "completion returns items")

    # 6. goto-definition of `helper` → jumps to its definition on line 0.
    c.send({"jsonrpc": "2.0", "id": 4, "method": "textDocument/definition",
            "params": {"textDocument": {"uri": uri},
                       "position": {"line": 6, "character": 12}}})
    c.pump(1.5)
    dfn = c.get_response(4)
    check(isinstance(dfn, dict) and dfn.get("range", {}).get("start", {}).get("line") == 0,
          "definition resolves to the declaration (line 0)")

    c.stop()
    if FAILS:
        print(f"\nlsp: FAIL ({len(FAILS)} check(s))")
        return 1
    print("\nlsp: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
