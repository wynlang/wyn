#!/usr/bin/env bash
# `wyn lsp` protocol regression test — drives the language server over stdio and
# asserts initialize/diagnostics/hover/completion/definition all work. Skips
# gracefully if python3 is unavailable (keeps CI green on minimal images).
set -u
WYN_BIN="${WYN:-./wyn}"
case "$WYN_BIN" in /*) ;; *) WYN_BIN="$(pwd)/$WYN_BIN" ;; esac

if ! command -v python3 >/dev/null 2>&1; then
    echo "lsp: SKIP (python3 not found)"
    exit 0
fi

WYN="$WYN_BIN" python3 "$(dirname "$0")/lsp_client.py"
