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
    /* Trust anchors. At least one source must be set; they combine. ca_file is a
     * path to a PEM bundle, ca_pem is PEM text already in memory. */
    const char* ca_file;
    const char* ca_pem;
    /* Also load the platform's root store - what an ordinary HTTPS call wants.
     * See wyn_tls_system_trust_count() for where roots come from per platform. */
    int use_system_trust;
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

/* How many root certificates the platform trust store yields here, or -1 with a
 * reason in err. Exposed so the failure is diagnosable ("0 roots found" is a very
 * different problem from "handshake failed") and so the gate can assert that
 * discovery works on the platform it is running on.
 *
 * Sources, in order: $SSL_CERT_FILE, $SSL_CERT_DIR, then the platform's usual
 * locations - the Windows "ROOT" store via CryptoAPI, /etc/ssl/cert.pem on
 * macOS/BSD, the distro bundles on Linux, /system/etc/security/cacerts on
 * Android. iOS ships no file and no enumerable store, so an iOS app must supply
 * ca_file or ca_pem (or set SSL_CERT_FILE); that is a documented gap, not a bug. */
long wyn_tls_system_trust_count(char* err, size_t errlen);

/* Send close_notify (best effort) and free everything. NULL is a no-op. */
void wyn_tls_close(WynTls* tls);

#endif /* WYN_TLS_H */
