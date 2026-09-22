/* wyn_tls.h - in-process TLS client seam.
 *
 * This exists to replace the HTTPS client's `popen("... | openssl s_client ...")`,
 * which spliced the caller's URL and POST body into a shell command. Everything
 * above this seam (Http.*, and later wyn_ai_call) speaks plain bytes; TLS,
 * certificate verification and hostname checking happen here.
 *
 * TWO PROPERTIES THIS SEAM GUARANTEES, because they are what the old code got wrong:
 *
 *   1. It FAILS CLOSED. There is no "skip verification" flag, not even for
 *      debugging. A connection needs trust material (WynTlsOptions.ca_file or
 *      .ca_pem) or it does not happen. `openssl s_client` verified nothing.
 *   2. No shell is involved, so no caller data can reach one.
 *
 * Backed by vendored mbedTLS 3.6 LTS (vendor/mbedtls/README.wyn.md).
 */
#ifndef WYN_TLS_H
#define WYN_TLS_H

#include <stddef.h>

typedef struct WynTls WynTls;

typedef struct {
    /* Trust anchors. At least one must be set; both may be. ca_file is a path to
     * a PEM bundle (what a system trust store looks like), ca_pem is PEM text
     * already in memory. */
    const char* ca_file;
    const char* ca_pem;
    /* Read timeout in milliseconds; 0 means block indefinitely. A network client
     * with no read timeout is a hang waiting to happen, so callers should set one. */
    unsigned int read_timeout_ms;
} WynTlsOptions;

/* Connect to host:port, handshake, and verify the peer against opt's trust
 * anchors (hostname checked against the certificate too). Returns NULL on any
 * failure, writing a human-readable reason into err (if err is non-NULL).
 *
 * host is also sent as SNI. port is a string ("443") to match getaddrinfo. */
WynTls* wyn_tls_connect(const char* host, const char* port,
                        const WynTlsOptions* opt, char* err, size_t errlen);

/* Write all of buf. Returns len, or -1 on error (see wyn_tls_error). */
long wyn_tls_write(WynTls* tls, const void* buf, size_t len);

/* Read up to len bytes. Returns the count, 0 at a clean end of stream
 * (close_notify or peer FIN), or -1 on error (see wyn_tls_error). */
long wyn_tls_read(WynTls* tls, void* buf, size_t len);

/* Last error on this connection; "" if none. Never NULL. */
const char* wyn_tls_error(const WynTls* tls);

/* Send close_notify (best effort) and free everything. NULL is a no-op. */
void wyn_tls_close(WynTls* tls);

#endif /* WYN_TLS_H */
