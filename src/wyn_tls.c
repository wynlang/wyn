/* wyn_tls.c - the TLS client seam. See wyn_tls.h for the contract. */

#include "wyn_tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

struct WynTls {
    mbedtls_net_context      net;
    mbedtls_ssl_context      ssl;
    mbedtls_ssl_config       conf;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt         cacert;
    char                     err[256];
};

/* mbedTLS returns negative codes; turn one into something a user can act on. */
static void tls_fail(char* buf, size_t buflen, const char* what, int ret)
{
    if (!buf || buflen == 0) return;
    char detail[128];
    mbedtls_strerror(ret, detail, sizeof(detail));
    snprintf(buf, buflen, "%s: %s (-0x%04x)", what, detail, (unsigned)-ret);
}

static void tls_free(WynTls* tls)
{
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_x509_crt_free(&tls->cacert);
    mbedtls_ctr_drbg_free(&tls->ctr_drbg);
    mbedtls_entropy_free(&tls->entropy);
    mbedtls_net_free(&tls->net);
    free(tls);
}

WynTls* wyn_tls_connect(const char* host, const char* port,
                        const WynTlsOptions* opt, char* err, size_t errlen)
{
    if (err && errlen) err[0] = '\0';

    if (!host || !*host || !port || !*port) {
        if (err) snprintf(err, errlen, "tls: host and port are required");
        return NULL;
    }
    /* Fail closed: no trust anchors, no connection. This is the whole point of
     * the seam - the code it replaces verified nothing at all. */
    if (!opt || (!opt->ca_file && !opt->ca_pem)) {
        if (err) snprintf(err, errlen,
                          "tls: no trust anchors configured (set ca_file or ca_pem); "
                          "refusing to connect without certificate verification");
        return NULL;
    }

    WynTls* tls = (WynTls*)calloc(1, sizeof(*tls));
    if (!tls) {
        if (err) snprintf(err, errlen, "tls: out of memory");
        return NULL;
    }

    mbedtls_net_init(&tls->net);
    mbedtls_ssl_init(&tls->ssl);
    mbedtls_ssl_config_init(&tls->conf);
    mbedtls_x509_crt_init(&tls->cacert);
    mbedtls_ctr_drbg_init(&tls->ctr_drbg);
    mbedtls_entropy_init(&tls->entropy);

    int ret;
    static const char* const pers = "wyn-tls-client";
    if ((ret = mbedtls_ctr_drbg_seed(&tls->ctr_drbg, mbedtls_entropy_func, &tls->entropy,
                                     (const unsigned char*)pers, strlen(pers))) != 0) {
        tls_fail(err, errlen, "tls: seeding the RNG failed", ret);
        tls_free(tls);
        return NULL;
    }

    if (opt->ca_file) {
        if ((ret = mbedtls_x509_crt_parse_file(&tls->cacert, opt->ca_file)) < 0) {
            tls_fail(err, errlen, "tls: reading the CA bundle failed", ret);
            tls_free(tls);
            return NULL;
        }
    }
    if (opt->ca_pem) {
        /* mbedTLS wants the terminating NUL counted for PEM input. */
        size_t pem_len = strlen(opt->ca_pem) + 1;
        if ((ret = mbedtls_x509_crt_parse(&tls->cacert, (const unsigned char*)opt->ca_pem,
                                          pem_len)) < 0) {
            tls_fail(err, errlen, "tls: parsing the CA PEM failed", ret);
            tls_free(tls);
            return NULL;
        }
    }

    if ((ret = mbedtls_ssl_config_defaults(&tls->conf, MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        tls_fail(err, errlen, "tls: config failed", ret);
        tls_free(tls);
        return NULL;
    }

    /* REQUIRED, stated explicitly rather than inherited from the preset: a future
     * preset change must not be able to turn verification off. */
    mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->cacert, NULL);
    mbedtls_ssl_conf_rng(&tls->conf, mbedtls_ctr_drbg_random, &tls->ctr_drbg);
    mbedtls_ssl_conf_min_tls_version(&tls->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    if (opt->read_timeout_ms > 0) {
        mbedtls_ssl_conf_read_timeout(&tls->conf, opt->read_timeout_ms);
    }

    if ((ret = mbedtls_ssl_setup(&tls->ssl, &tls->conf)) != 0) {
        tls_fail(err, errlen, "tls: setup failed", ret);
        tls_free(tls);
        return NULL;
    }
    /* Sets SNI *and* the name the certificate is checked against. */
    if ((ret = mbedtls_ssl_set_hostname(&tls->ssl, host)) != 0) {
        tls_fail(err, errlen, "tls: setting the hostname failed", ret);
        tls_free(tls);
        return NULL;
    }

    if ((ret = mbedtls_net_connect(&tls->net, host, port, MBEDTLS_NET_PROTO_TCP)) != 0) {
        tls_fail(err, errlen, "tls: connect failed", ret);
        tls_free(tls);
        return NULL;
    }
    /* recv is NULL and recv_timeout carries the read: that is mbedTLS's documented
     * pairing when a read timeout is configured. */
    mbedtls_ssl_set_bio(&tls->ssl, &tls->net, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);

    while ((ret = mbedtls_ssl_handshake(&tls->ssl)) != 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
            /* The actionable case: say WHICH check failed, not just "handshake failed". */
            char why[192];
            uint32_t flags = mbedtls_ssl_get_verify_result(&tls->ssl);
            mbedtls_x509_crt_verify_info(why, sizeof(why), "", flags);
            size_t wlen = strlen(why);
            while (wlen > 0 && (why[wlen - 1] == '\n' || why[wlen - 1] == ' ')) why[--wlen] = '\0';
            if (err) snprintf(err, errlen, "tls: certificate verification failed: %s",
                              wlen ? why : "unknown reason");
        } else {
            tls_fail(err, errlen, "tls: handshake failed", ret);
        }
        tls_free(tls);
        return NULL;
    }

    return tls;
}

long wyn_tls_write(WynTls* tls, const void* buf, size_t len)
{
    if (!tls || !buf) return -1;
    const unsigned char* p = (const unsigned char*)buf;
    size_t sent = 0;
    while (sent < len) {
        int ret = mbedtls_ssl_write(&tls->ssl, p + sent, len - sent);
        if (ret > 0) { sent += (size_t)ret; continue; }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        tls_fail(tls->err, sizeof(tls->err), "tls: write failed", ret);
        return -1;
    }
    return (long)sent;
}

long wyn_tls_read(WynTls* tls, void* buf, size_t len)
{
    if (!tls || !buf) return -1;
    if (len == 0) return 0;
    for (;;) {
        int ret = mbedtls_ssl_read(&tls->ssl, (unsigned char*)buf, len);
        if (ret > 0) return (long)ret;
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        /* A server that closes without close_notify is ordinary in the wild, so
         * both endings are a clean 0 rather than an error. */
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) return 0;
        if (ret == MBEDTLS_ERR_NET_CONN_RESET) return 0;
        tls_fail(tls->err, sizeof(tls->err), "tls: read failed", ret);
        return -1;
    }
}

const char* wyn_tls_error(const WynTls* tls)
{
    return tls ? tls->err : "";
}

void wyn_tls_close(WynTls* tls)
{
    if (!tls) return;
    int ret;
    do { ret = mbedtls_ssl_close_notify(&tls->ssl); }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    tls_free(tls);
}
