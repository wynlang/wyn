/* wyn_https.c - native HTTPS over the TLS seam. See wyn_https.h for the contract
 * and for what this replaces (a popen'd `openssl s_client` with the caller's URL
 * spliced into the command). */

#include "wyn_https.h"
#include "wyn_tls.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A network client with no read timeout is a hang waiting to happen. */
#define WYN_HTTPS_TIMEOUT_MS_DEFAULT 30000u
/* An explicit ceiling, not a cap that lies: reaching it is an ERROR with a message,
 * where the code this replaces silently truncated at 128 KB. It exists because a
 * hostile or broken server can otherwise stream until the process is OOM-killed -
 * the same class as the remote-JSON DoS fixed in #307. */
#define WYN_HTTPS_MAX_BYTES_DEFAULT ((size_t)64 * 1024 * 1024)
#define WYN_HTTPS_READ_CHUNK        (64u * 1024u)

static void https_err(char* err, size_t errlen, const char* fmt, ...)
{
    if (!err || errlen == 0) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, errlen, fmt, ap);
    va_end(ap);
}

static size_t env_size(const char* name, size_t fallback)
{
    const char* v = getenv(name);
    if (!v || !*v) return fallback;
    char*         end = NULL;
    unsigned long n   = strtoul(v, &end, 10);
    if (n == 0 || (end && *end)) return fallback;
    return (size_t)n;
}

/* Control characters are what turns one request into two. Tab is legal inside a
 * header value; everything else below 0x20, plus DEL, is not. */
static int has_ctl(const char* s, size_t n, int allow_tab)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\t' && allow_tab) continue;
        if (c < 0x20 || c == 0x7f) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------ growable buffer */

typedef struct {
    char*  p;
    size_t len;
    size_t cap;
} Buf;

static int buf_reserve(Buf* b, size_t extra)
{
    if (b->cap - b->len > extra) return 0;             /* keep room for a NUL */
    size_t want = b->cap ? b->cap : 8192;
    while (want - b->len <= extra) {
        if (want > (size_t)-1 / 2) return -1;
        want *= 2;
    }
    char* np = (char*)realloc(b->p, want);
    if (!np) return -1;
    b->p   = np;
    b->cap = want;
    return 0;
}

static int buf_add(Buf* b, const char* s, size_t n)
{
    if (buf_reserve(b, n) != 0) return -1;
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
    return 0;
}

static int buf_str(Buf* b, const char* s) { return buf_add(b, s, strlen(s)); }

static int buf_fmt(Buf* b, const char* fmt, ...)
{
    char    line[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof(line)) return -1;
    return buf_add(b, line, (size_t)n);
}

static void buf_free(Buf* b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }

/* --------------------------------------------------------------- byte search */

/* No memmem: it is a GNU extension, and this file compiles on Windows and macOS
 * too. Bodies can contain NUL, so nothing here may use strstr on the payload. */
static const char* find_bytes(const char* hay, size_t hlen, const char* needle, size_t nlen)
{
    if (nlen == 0 || hlen < nlen) return NULL;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        if (hay[i] == needle[0] && memcmp(hay + i, needle, nlen) == 0) return hay + i;
    }
    return NULL;
}

static int ci_eq(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return 1;
}

/* Find a header's value inside a block of header lines. LINE-ANCHORED on purpose:
 * the old code did strstr(response, "Transfer-Encoding: chunked") over the WHOLE
 * response, so a body that merely mentioned the phrase flipped the decoder on.
 *
 * skip_status_line is 1 for a RESPONSE block (its first line is "HTTP/1.1 200 OK",
 * which has no Name: value shape) and 0 for a caller-supplied request block, whose
 * very first line is already a header. Getting that wrong is silent: a lookup that
 * skips the only line present simply never matches. Returns a pointer into `block`
 * with *vlen set, or NULL. */
static const char* hdr_get(const char* block, size_t blen, const char* name,
                           size_t* vlen, int skip_status_line)
{
    size_t nlen = strlen(name);
    size_t i    = 0;
    if (skip_status_line) {
        const char* eol = find_bytes(block, blen, "\r\n", 2);
        if (!eol) return NULL;
        i = (size_t)(eol - block) + 2;
    }

    while (i < blen) {
        size_t      rest = blen - i;
        const char* e    = find_bytes(block + i, rest, "\r\n", 2);
        size_t      llen = e ? (size_t)(e - (block + i)) : rest;
        if (llen > nlen && ci_eq(block + i, name, nlen) && block[i + nlen] == ':') {
            const char* v = block + i + nlen + 1;
            size_t      n = llen - nlen - 1;
            while (n > 0 && (*v == ' ' || *v == '\t')) { v++; n--; }
            while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) n--;
            if (vlen) *vlen = n;
            return v;
        }
        if (!e) break;
        i += llen + 2;
    }
    return NULL;
}

/* ---------------------------------------------------------------- validation */

static int method_ok(const char* m)
{
    static const char* const allowed[] = { "GET", "POST", "PUT", "DELETE",
                                           "PATCH", "HEAD", NULL };
    for (int i = 0; allowed[i]; i++) if (strcmp(m, allowed[i]) == 0) return 1;
    return 0;
}

/* Each line must be `Name: value\r\n` with a token name and no control character
 * in the value. This is where an http_set_header(key, val) whose VALUE carries a
 * bare CR or LF is stopped - otherwise it appends a second request. */
static int headers_ok(const char* h, char* err, size_t errlen)
{
    size_t total = strlen(h);
    size_t i     = 0;
    while (i < total) {
        const char* e = strstr(h + i, "\r\n");
        if (!e) {
            https_err(err, errlen,
                      "https: invalid header block (a header line is not terminated "
                      "by CRLF)");
            return 0;
        }
        size_t      llen  = (size_t)(e - (h + i));
        const char* line  = h + i;
        const char* colon = NULL;
        for (size_t k = 0; k < llen; k++) if (line[k] == ':') { colon = line + k; break; }
        if (llen == 0 || !colon || colon == line) {
            https_err(err, errlen, "https: invalid header line (expected \"Name: value\")");
            return 0;
        }
        size_t nlen = (size_t)(colon - line);
        for (size_t k = 0; k < nlen; k++) {
            unsigned char c = (unsigned char)line[k];
            if (c <= 0x20 || c >= 0x7f || c == ':' || c == '(' || c == ')' ||
                c == ',' || c == '/' || c == ';' || c == '<' || c == '=' ||
                c == '>' || c == '?' || c == '@' || c == '[' || c == '\\' ||
                c == ']' || c == '{' || c == '}' || c == '"') {
                https_err(err, errlen, "https: invalid character in header name");
                return 0;
            }
        }
        if (has_ctl(colon + 1, llen - nlen - 1, /*allow_tab=*/1)) {
            https_err(err, errlen,
                      "https: control character in a header value - refusing to send "
                      "a request that could be split in two");
            return 0;
        }
        i += llen + 2;
    }
    return 1;
}

/* ------------------------------------------------------------------ chunked */

/* Decode Transfer-Encoding: chunked into out. Tolerant of a bare LF terminator
 * (servers in the wild send it) and of chunk extensions after a ";". */
static int dechunk(const char* p, size_t avail, Buf* out, char* err, size_t errlen)
{
    const char* end = p + avail;
    for (;;) {
        /* The size line: hex digits, then an optional ";ext", then CRLF. */
        const char* q    = p;
        size_t      size = 0;
        int         digits = 0;
        while (q < end) {
            char c = *q;
            int  d;
            if (c >= '0' && c <= '9')      d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else break;
            if (size > ((size_t)-1 - (size_t)d) / 16) {
                https_err(err, errlen, "https: chunk size overflows");
                return -1;
            }
            size = size * 16 + (size_t)d;
            digits++;
            q++;
        }
        if (digits == 0) {
            https_err(err, errlen, "https: malformed chunked response (no chunk size)");
            return -1;
        }
        while (q < end && *q != '\n') q++;      /* skips any ";extension" and the CR */
        if (q >= end) {
            https_err(err, errlen, "https: truncated chunked response (no end of size line)");
            return -1;
        }
        q++;                                     /* past the LF */

        if (size == 0) return 0;                 /* terminator; trailers ignored */
        if ((size_t)(end - q) < size) {
            https_err(err, errlen,
                      "https: truncated chunked response (chunk claims %zu bytes, %zu left)",
                      size, (size_t)(end - q));
            return -1;
        }
        if (buf_add(out, q, size) != 0) {
            https_err(err, errlen, "https: out of memory assembling the response");
            return -1;
        }
        p = q + size;
        /* The CRLF that follows the data. Be lenient about which form. */
        if (p < end && *p == '\r') p++;
        if (p < end && *p == '\n') p++;
    }
}

/* -------------------------------------------------------------- the request */

void wyn_https_response_free(WynHttpResponse* r)
{
    if (!r) return;
    free(r->headers);
    free(r->body);
    r->headers  = NULL;
    r->body     = NULL;
    r->body_len = 0;
    r->status   = 0;
}

int wyn_https_request(const char* method, const char* url, const char* body,
                      const char* extra_headers, WynHttpResponse* out,
                      char* err, size_t errlen)
{
    if (err && errlen) err[0] = '\0';
    if (!out) {
        https_err(err, errlen, "https: no output struct");
        return -1;
    }
    memset(out, 0, sizeof(*out));

    if (!method || !method_ok(method)) {
        https_err(err, errlen, "https: unsupported method \"%s\"", method ? method : "(null)");
        return -1;
    }
    if (!url || strncmp(url, "https://", 8) != 0) {
        https_err(err, errlen, "https: URL must start with \"https://\"");
        return -1;
    }
    /* One check for the whole URL before it is taken apart: a control character
     * anywhere in it is a request-splitting attempt, and this is the direct
     * replacement for "the URL was spliced into a shell command". */
    size_t urllen = strlen(url);
    if (has_ctl(url, urllen, /*allow_tab=*/0)) {
        https_err(err, errlen,
                  "https: control character in the URL - refusing to send a request "
                  "that could be split in two");
        return -1;
    }
    if (extra_headers && *extra_headers && !headers_ok(extra_headers, err, errlen)) {
        return -1;
    }

    /* --- host, port, path ------------------------------------------------- */
    char        host[256];
    char        port[16] = "443";
    const char* p        = url + 8;
    const char* hend;

    if (*p == '[') {                                  /* IPv6 literal */
        const char* close_br = strchr(p, ']');
        if (!close_br) {
            https_err(err, errlen, "https: unterminated IPv6 literal in the URL");
            return -1;
        }
        size_t hlen = (size_t)(close_br - p - 1);
        if (hlen == 0 || hlen >= sizeof(host)) {
            https_err(err, errlen, "https: host in the URL is empty or too long");
            return -1;
        }
        memcpy(host, p + 1, hlen);
        host[hlen] = '\0';
        hend       = close_br + 1;
        if (*hend == ':') {
            const char* ps = hend + 1;
            size_t      pl = strcspn(ps, "/?#");
            if (pl == 0 || pl >= sizeof(port)) {
                https_err(err, errlen, "https: invalid port in the URL");
                return -1;
            }
            memcpy(port, ps, pl);
            port[pl] = '\0';
            hend     = ps + pl;
        }
    } else {
        size_t auth = strcspn(p, "/?#");
        size_t hlen = auth;
        for (size_t i = 0; i < auth; i++) {
            if (p[i] == ':') { hlen = i; break; }
        }
        if (hlen == 0 || hlen >= sizeof(host)) {
            https_err(err, errlen, "https: host in the URL is empty or too long");
            return -1;
        }
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        if (hlen < auth) {                            /* an explicit :port */
            size_t pl = auth - hlen - 1;
            if (pl == 0 || pl >= sizeof(port)) {
                https_err(err, errlen, "https: invalid port in the URL");
                return -1;
            }
            memcpy(port, p + hlen + 1, pl);
            port[pl] = '\0';
        }
        hend = p + auth;
    }
    for (const char* c = port; *c; c++) {
        if (*c < '0' || *c > '9') {
            https_err(err, errlen, "https: invalid port \"%s\" in the URL", port);
            return -1;
        }
    }

    const char* path = (*hend == '\0') ? "/" : hend;

    /* --- connect ---------------------------------------------------------- */
    WynTlsOptions opt;
    memset(&opt, 0, sizeof(opt));
    opt.use_system_trust = 1;                /* never off; the seam fails closed */
    opt.read_timeout_ms  = (unsigned int)env_size("WYN_HTTPS_TIMEOUT_MS",
                                                  WYN_HTTPS_TIMEOUT_MS_DEFAULT);

    char tls_err[320] = {0};
    WynTls* tls = wyn_tls_connect(host, port, &opt, tls_err, sizeof(tls_err));
    if (!tls) {
        /* Verbatim: the seam's message already names WHICH check failed, and
         * callers (and the gate) match on it. */
        https_err(err, errlen, "%s", tls_err[0] ? tls_err : "tls: connection failed");
        return -1;
    }

    /* --- send ------------------------------------------------------------- */
    Buf   req = {NULL, 0, 0};
    int   rc  = 0;
    size_t blen = body ? strlen(body) : 0;

    rc |= buf_fmt(&req, "%s %s HTTP/1.1\r\n", method, path);
    if (strcmp(port, "443") == 0) rc |= buf_fmt(&req, "Host: %s\r\n", host);
    else                          rc |= buf_fmt(&req, "Host: %s:%s\r\n", host, port);
    rc |= buf_str(&req, "User-Agent: wyn\r\n");
    rc |= buf_str(&req, "Accept: */*\r\n");
    /* Connection: close is what makes "read to end of stream" a correct framing
     * for the no-Content-Length case. Keep-alive is a separate concern. */
    rc |= buf_str(&req, "Connection: close\r\n");
    if (extra_headers && *extra_headers) rc |= buf_str(&req, extra_headers);
    if (body) {
        rc |= buf_fmt(&req, "Content-Length: %zu\r\n", blen);
        size_t ctlen = 0;
        /* Only a DEFAULT: an explicit Content-Type from the caller wins. */
        if (!extra_headers || !*extra_headers ||
            hdr_get(extra_headers, strlen(extra_headers), "Content-Type", &ctlen,
                    /*skip_status_line=*/0) == NULL) {
            rc |= buf_str(&req, "Content-Type: application/x-www-form-urlencoded\r\n");
        }
    }
    rc |= buf_str(&req, "\r\n");
    if (body && blen) rc |= buf_add(&req, body, blen);
    if (rc != 0) {
        https_err(err, errlen, "https: could not build the request (URL or header too long)");
        buf_free(&req);
        wyn_tls_close(tls);
        return -1;
    }

    if (wyn_tls_write(tls, req.p, req.len) != (long)req.len) {
        https_err(err, errlen, "https: sending the request failed: %s", wyn_tls_error(tls));
        buf_free(&req);
        wyn_tls_close(tls);
        return -1;
    }
    buf_free(&req);

    /* --- read the whole response, NO fixed cap ---------------------------- */
    size_t max_bytes = env_size("WYN_HTTPS_MAX_BYTES", WYN_HTTPS_MAX_BYTES_DEFAULT);
    Buf    resp      = {NULL, 0, 0};
    for (;;) {
        if (buf_reserve(&resp, WYN_HTTPS_READ_CHUNK) != 0) {
            https_err(err, errlen, "https: out of memory reading the response");
            buf_free(&resp);
            wyn_tls_close(tls);
            return -1;
        }
        long n = wyn_tls_read(tls, resp.p + resp.len, resp.cap - resp.len - 1);
        if (n < 0) {
            https_err(err, errlen, "https: reading the response failed: %s", wyn_tls_error(tls));
            buf_free(&resp);
            wyn_tls_close(tls);
            return -1;
        }
        if (n == 0) break;
        resp.len += (size_t)n;
        resp.p[resp.len] = '\0';
        if (resp.len > max_bytes) {
            https_err(err, errlen,
                      "https: response exceeds %zu bytes (raise WYN_HTTPS_MAX_BYTES if "
                      "that is really expected)", max_bytes);
            buf_free(&resp);
            wyn_tls_close(tls);
            return -1;
        }
    }
    wyn_tls_close(tls);

    if (resp.len == 0) {
        https_err(err, errlen, "https: the server closed the connection without replying");
        buf_free(&resp);
        return -1;
    }

    /* --- status line ------------------------------------------------------ */
    if (resp.len < 12 || memcmp(resp.p, "HTTP/", 5) != 0) {
        https_err(err, errlen, "https: the reply is not an HTTP response");
        buf_free(&resp);
        return -1;
    }
    {
        const char* sp = NULL;
        for (size_t i = 0; i < resp.len && i < 64; i++) {
            if (resp.p[i] == ' ') { sp = resp.p + i; break; }
            if (resp.p[i] == '\r' || resp.p[i] == '\n') break;
        }
        if (!sp) {
            https_err(err, errlen, "https: malformed status line");
            buf_free(&resp);
            return -1;
        }
        out->status = (int)strtol(sp + 1, NULL, 10);
    }

    /* --- header / body split ---------------------------------------------- */
    size_t      hlen   = resp.len;      /* header block length, terminator excluded */
    size_t      boff   = resp.len;      /* where the body starts */
    const char* sep    = find_bytes(resp.p, resp.len, "\r\n\r\n", 4);
    if (sep) {
        hlen = (size_t)(sep - resp.p);
        boff = hlen + 4;
    } else if ((sep = find_bytes(resp.p, resp.len, "\n\n", 2)) != NULL) {
        hlen = (size_t)(sep - resp.p);
        boff = hlen + 2;
    }

    out->headers = (char*)malloc(hlen + 1);
    if (!out->headers) {
        https_err(err, errlen, "https: out of memory");
        buf_free(&resp);
        return -1;
    }
    memcpy(out->headers, resp.p, hlen);
    out->headers[hlen] = '\0';

    const char* raw   = resp.p + boff;
    size_t      avail = resp.len - boff;

    /* --- framing: chunked, Content-Length, or to end of stream ------------- */
    size_t      tlen = 0;
    const char* te   = hdr_get(resp.p, hlen, "Transfer-Encoding", &tlen, 1);
    int         chunked = 0;
    if (te) {
        /* "chunked", or the last entry of a list like "gzip, chunked". */
        for (size_t i = 0; i + 7 <= tlen; i++) {
            if (ci_eq(te + i, "chunked", 7)) { chunked = 1; break; }
        }
    }

    if (chunked) {
        Buf decoded = {NULL, 0, 0};
        if (dechunk(raw, avail, &decoded, err, errlen) != 0) {
            buf_free(&decoded);
            buf_free(&resp);
            free(out->headers);
            out->headers = NULL;
            return -1;
        }
        if (!decoded.p) {                       /* a legitimately empty body */
            decoded.p = (char*)malloc(1);
            if (decoded.p) decoded.p[0] = '\0';
        }
        out->body     = decoded.p;
        out->body_len = decoded.len;
    } else {
        size_t      clen  = 0;
        size_t      want  = avail;
        const char* cl    = hdr_get(resp.p, hlen, "Content-Length", &clen, 1);
        if (cl && clen > 0 && clen < 32) {
            char numbuf[32];
            memcpy(numbuf, cl, clen);
            numbuf[clen] = '\0';
            unsigned long n = strtoul(numbuf, NULL, 10);
            /* A Content-Length longer than what arrived means the peer went away
             * mid-body; return what there is rather than inventing bytes. */
            want = ((size_t)n < avail) ? (size_t)n : avail;
        }
        if (strcmp(method, "HEAD") == 0) want = 0;
        out->body = (char*)malloc(want + 1);
        if (!out->body) {
            https_err(err, errlen, "https: out of memory");
            buf_free(&resp);
            free(out->headers);
            out->headers = NULL;
            return -1;
        }
        if (want) memcpy(out->body, raw, want);
        out->body[want] = '\0';
        out->body_len   = want;
    }

    buf_free(&resp);
    if (!out->body) {
        https_err(err, errlen, "https: out of memory");
        free(out->headers);
        out->headers = NULL;
        return -1;
    }
    return 0;
}
