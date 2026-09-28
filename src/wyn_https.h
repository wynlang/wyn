/* wyn_https.h - the native HTTPS transport, one layer above the TLS seam.
 *
 * WHAT THIS REPLACES, and why each part of the contract exists. The previous
 * implementation (src/wyn_runtime.h, https_get / https_post / the https branches of
 * http_put / http_delete) did:
 *
 *   popen("printf 'GET <url> HTTP/1.1...<body>' | openssl s_client -connect <host>:443")
 *
 * which was four bugs at once:
 *
 *   1. REMOTE CODE EXECUTION. The URL and the POST body were interpolated into a
 *      shell command. Nothing here touches a shell, and the request target and any
 *      caller-supplied header are REJECTED if they contain a control character, so
 *      a request cannot be split either.
 *   2. A HARD 128 KB CAP (wyn_malloc(131072) + fread(..., 131071)). A real 1.3 MB
 *      response came back as 126 KB, silently. This grows its buffer instead; the
 *      only limit is an explicit DoS ceiling that REPORTS when it is hit
 *      ($WYN_HTTPS_MAX_BYTES, default 64 MB).
 *   3. NO CHUNKED DECODING, so chunk-length lines arrived inside the body.
 *      Transfer-Encoding: chunked and Content-Length are both decoded here.
 *   4. THE STATUS WAS UNREACHABLE - headers were cut at \r\n\r\n and discarded, so a
 *      500 and a 200 were indistinguishable. Both the status line and the header
 *      block come back.
 *
 * Verification is NOT optional and there is no flag to turn it off: the transport
 * always asks wyn_tls_connect for the platform trust store, and that seam fails
 * closed. $SSL_CERT_FILE overrides the store (the documented way to supply roots in
 * a container or on iOS), which is also how tests/https/test_https_request.c drives
 * this against a private CA without a test-only back door.
 *
 * This is deliberately NOT the language surface. Http.get() still returns a plain
 * string; the typed Result<HttpResponse, HttpError> surface is a separate change.
 * What lands here is the transport plus enough structure (status, headers) for that
 * later work to have something to expose.
 */
#ifndef WYN_HTTPS_H
#define WYN_HTTPS_H

#include <stddef.h>

typedef struct {
    /* Status code from the response's status line. 0 only if the peer sent
     * something that is not an HTTP response at all (which is an error return). */
    int    status;
    /* The response header block, NUL-terminated, status line included, WITHOUT the
     * blank line that ends it. malloc'd, may be NULL if there were none. */
    char*  headers;
    /* The decoded body: body_len bytes plus a NUL terminator, so it is safe to use
     * as a C string when it happens to be text, and exact when it is not. malloc'd. */
    char*  body;
    size_t body_len;
} WynHttpResponse;

/* One HTTPS round trip. Returns 0 on success (out is filled, caller must call
 * wyn_https_response_free), or -1 with a human-readable reason in err.
 *
 *   method        "GET", "POST", "PUT", "DELETE", "PATCH" or "HEAD".
 *   url           must start with "https://". host[:port][/path][?query], and an
 *                 IPv6 literal in brackets is understood. Default port 443.
 *   body          NULL for no body; otherwise sent with a Content-Length.
 *   extra_headers NULL, or zero or more "Name: value\r\n" lines. Each line is
 *                 validated; an injected CR/LF is refused rather than sent.
 *
 * A non-2xx response is NOT an error - it is a successful round trip with that
 * status, which is the whole point of item 4 above.
 */
int wyn_https_request(const char* method, const char* url, const char* body,
                      const char* extra_headers, WynHttpResponse* out,
                      char* err, size_t errlen);

/* Frees what wyn_https_request allocated and zeroes the struct. NULL is a no-op,
 * and calling it twice is safe. */
void wyn_https_response_free(WynHttpResponse* r);

#endif /* WYN_HTTPS_H */
