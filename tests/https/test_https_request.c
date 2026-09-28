/* test_https_request.c - gate for src/wyn_https.c, the native HTTPS transport.
 *
 * It replaces `popen("printf '...' | openssl s_client -connect host:443")`, so the
 * things this file checks are exactly the things that code got wrong:
 *
 *   1. a 128 KB hard cap (wyn_malloc(131072) + fread(...,131071)) that silently
 *      truncated a 1.3 MB response to 126 KB - arm C sends 250 KB;
 *   2. no chunked decoding, so chunk-length lines arrived INSIDE the body - arm B
 *      sends three chunks, one of them 10 KB;
 *   3. headers cut at \r\n\r\n and thrown away, so a 500 and a 200 were
 *      indistinguishable - arm D asks for a 404 and expects to be told;
 *   4. no certificate verification of any kind - arm E points the trust store at
 *      an unrelated CA and requires a refusal.
 *
 * Same self-contained shape as tests/tls/test_tls_seam.c, and for the same reasons:
 * the CA and leaf certificates are MINTED AT RUNTIME (no private key is ever
 * committed, and nothing expires on a future maintainer), the port comes from the
 * OS, and everything is loopback - no network egress and no openssl CLI.
 *
 * The cert-minting helpers mirror tests/tls/test_tls_seam.c rather than being
 * shared with it: factoring them out would mean editing that already-green gate
 * inside this PR, and its own comments explain the load-bearing details.
 *
 * TRUST, WITHOUT A TEST-ONLY BACK DOOR: wyn_https_request always asks for
 * use_system_trust, and wyn_tls.c treats $SSL_CERT_FILE as AUTHORITATIVE over the
 * platform store. So pointing SSL_CERT_FILE at the minted CA makes the private CA
 * the only root - exercising the production path byte for byte, with no "skip
 * verification" flag to add (the seam deliberately has none).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "wyn_https.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

static int failures = 0;
static int checks   = 0;

static void ok(const char* name) { printf("  PASS: %s\n", name); checks++; }
static void bad(const char* name, const char* detail)
{
    printf("  FAIL: %s\n        %s\n", name, detail ? detail : "");
    failures++;
    checks++;
}

/* ------------------------------------------------- certificates, minted here */

typedef struct {
    mbedtls_pk_context key;
    char               crt_pem[4096];
    char               key_pem[4096];
    char               dn[128];
} Ident;

static mbedtls_entropy_context  g_entropy;
static mbedtls_ctr_drbg_context g_rng;

static int ident_init(Ident* id, const char* cn)
{
    memset(id, 0, sizeof(*id));
    snprintf(id->dn, sizeof(id->dn), "CN=%s", cn);
    mbedtls_pk_init(&id->key);
    if (mbedtls_pk_setup(&id->key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) return -1;
    if (mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(id->key),
                            mbedtls_ctr_drbg_random, &g_rng) != 0) return -1;
    return mbedtls_pk_write_key_pem(&id->key, (unsigned char*)id->key_pem, sizeof(id->key_pem));
}

static int ident_issue(Ident* id, Ident* issuer, int is_ca)
{
    mbedtls_x509write_cert crt;
    mbedtls_x509write_crt_init(&crt);
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &id->key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &issuer->key);

    int ret = mbedtls_x509write_crt_set_subject_name(&crt, id->dn);
    if (ret == 0) ret = mbedtls_x509write_crt_set_issuer_name(&crt, issuer->dn);
    if (ret == 0) ret = mbedtls_x509write_crt_set_validity(&crt, "20200101000000", "20991231235959");
    if (ret == 0) ret = mbedtls_x509write_crt_set_basic_constraints(&crt, is_ca, is_ca ? 0 : -1);
    if (ret == 0) {
        unsigned char serial[8] = {0, 0, 0, 0, 0, 0, 0, 1};
        serial[7] = (unsigned char)(is_ca ? 1 : 2);
        ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial));
    }
    if (ret == 0) ret = mbedtls_x509write_crt_pem(&crt, (unsigned char*)id->crt_pem,
                                                  sizeof(id->crt_pem),
                                                  mbedtls_ctr_drbg_random, &g_rng);
    mbedtls_x509write_crt_free(&crt);
    return ret;
}

static void ident_free(Ident* id) { mbedtls_pk_free(&id->key); }

/* ------------------------------------------------------- the bodies we expect */

#define SMALL_BODY "wyn native https, no shell involved\n"

/* Chunk 2 is 10000 bytes on purpose: bigger than any single TLS record and bigger
 * than the 8 KB read buffer, so a decoder that assumes one chunk arrives in one
 * read fails here rather than in production. Content, not just length, is checked. */
#define BIG_CHUNK_LEN 10000
static char* make_big_chunk(void)
{
    char* p = (char*)malloc(BIG_CHUNK_LEN + 1);
    for (int i = 0; i < BIG_CHUNK_LEN; i++) p[i] = (char)('a' + (i % 26));
    p[BIG_CHUNK_LEN] = '\0';
    return p;
}

/* 250000 > 200 KB > the old 131071-byte fread cap. */
#define BIG_BODY_LEN 250000
static char* make_big_body(void)
{
    char* p = (char*)malloc(BIG_BODY_LEN + 1);
    for (int i = 0; i < BIG_BODY_LEN; i++) p[i] = (char)('0' + (i % 10));
    p[BIG_BODY_LEN] = '\0';
    return p;
}

/* ---------------------------------------------------------------- the server */

typedef struct {
    mbedtls_net_context listen_ctx;
    const Ident*        good;
    volatile int        stop;
} Server;

static int ssl_write_all(mbedtls_ssl_context* ssl, const char* p, size_t len)
{
    while (len > 0) {
        int ret = mbedtls_ssl_write(ssl, (const unsigned char*)p, len);
        if (ret > 0) { p += ret; len -= (size_t)ret; continue; }
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) return -1;
    }
    return 0;
}

/* Read the request head, then answer according to its path. Keeping the routing
 * in the PATH rather than in an accept counter means an arm that correctly refuses
 * to connect cannot silently shift every later arm's response by one - the failure
 * mode that the TLS seam test documents. */
static void serve_one(mbedtls_net_context* client_in, const Ident* id)
{
    mbedtls_net_context      client = *client_in;
    mbedtls_ssl_context      ssl;
    mbedtls_ssl_config       conf;
    mbedtls_x509_crt         srvcrt;
    mbedtls_pk_context       pkey;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context rng;
    char*                    big_chunk = NULL;
    char*                    big_body  = NULL;

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&srvcrt);
    mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&rng);

    static const char* const pers = "wyn-https-test-server";
    if (mbedtls_ctr_drbg_seed(&rng, mbedtls_entropy_func, &entropy,
                              (const unsigned char*)pers, strlen(pers)) != 0) goto done;
    if (mbedtls_x509_crt_parse(&srvcrt, (const unsigned char*)id->crt_pem,
                               strlen(id->crt_pem) + 1) != 0) goto done;
    if (mbedtls_pk_parse_key(&pkey, (const unsigned char*)id->key_pem, strlen(id->key_pem) + 1,
                             NULL, 0, mbedtls_ctr_drbg_random, &rng) != 0) goto done;
    if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) goto done;
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &rng);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    if (mbedtls_ssl_conf_own_cert(&conf, &srvcrt, &pkey) != 0) goto done;
    if (mbedtls_ssl_setup(&ssl, &conf) != 0) goto done;

    mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, NULL);

    {
        int ret;
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) goto done;
        }

        /* Read until the end of the header block, then as much body as the request
         * declares. A short read here would make the echo arm flaky, not wrong. */
        char   req[65536];
        size_t got = 0;
        char*  sep = NULL;
        while (got < sizeof(req) - 1) {
            ret = mbedtls_ssl_read(&ssl, (unsigned char*)req + got, sizeof(req) - 1 - got);
            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            if (ret <= 0) break;
            got += (size_t)ret;
            req[got] = '\0';
            sep = strstr(req, "\r\n\r\n");
            if (!sep) continue;
            const char* cl = strstr(req, "Content-Length:");
            if (!cl) break;
            size_t want = (size_t)strtoul(cl + 15, NULL, 10);
            size_t have = got - (size_t)(sep + 4 - req);
            if (have >= want) break;
        }
        if (!sep) goto done;
        req[got] = '\0';

        /* The path is the second token of the request line. */
        char path[512] = {0};
        {
            const char* sp1 = strchr(req, ' ');
            const char* sp2 = sp1 ? strchr(sp1 + 1, ' ') : NULL;
            if (sp1 && sp2 && (size_t)(sp2 - sp1 - 1) < sizeof(path)) {
                memcpy(path, sp1 + 1, (size_t)(sp2 - sp1 - 1));
            }
        }

        char head[256];
        if (strcmp(path, "/small") == 0) {
            snprintf(head, sizeof(head), "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                                         "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                     strlen(SMALL_BODY));
            if (ssl_write_all(&ssl, head, strlen(head)) != 0) goto done;
            if (ssl_write_all(&ssl, SMALL_BODY, strlen(SMALL_BODY)) != 0) goto done;
        } else if (strcmp(path, "/chunked") == 0) {
            big_chunk = make_big_chunk();
            snprintf(head, sizeof(head), "HTTP/1.1 200 OK\r\n"
                                         "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n");
            if (ssl_write_all(&ssl, head, strlen(head)) != 0) goto done;
            char sz[32];
            /* Chunk 1 */
            snprintf(sz, sizeof(sz), "%X\r\n", 6);
            if (ssl_write_all(&ssl, sz, strlen(sz)) != 0) goto done;
            if (ssl_write_all(&ssl, "alpha-\r\n", 8) != 0) goto done;
            /* Chunk 2: 10 KB, with a chunk extension to prove the ";..." is skipped */
            snprintf(sz, sizeof(sz), "%X;note=big\r\n", BIG_CHUNK_LEN);
            if (ssl_write_all(&ssl, sz, strlen(sz)) != 0) goto done;
            if (ssl_write_all(&ssl, big_chunk, BIG_CHUNK_LEN) != 0) goto done;
            if (ssl_write_all(&ssl, "\r\n", 2) != 0) goto done;
            /* Chunk 3 */
            snprintf(sz, sizeof(sz), "%X\r\n", 6);
            if (ssl_write_all(&ssl, sz, strlen(sz)) != 0) goto done;
            if (ssl_write_all(&ssl, "-omega\r\n", 8) != 0) goto done;
            /* Terminator + an empty trailer section */
            if (ssl_write_all(&ssl, "0\r\n\r\n", 5) != 0) goto done;
        } else if (strcmp(path, "/big") == 0) {
            big_body = make_big_body();
            snprintf(head, sizeof(head), "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n"
                                         "Connection: close\r\n\r\n", BIG_BODY_LEN);
            if (ssl_write_all(&ssl, head, strlen(head)) != 0) goto done;
            if (ssl_write_all(&ssl, big_body, BIG_BODY_LEN) != 0) goto done;
        } else if (strcmp(path, "/echo") == 0) {
            /* STRICT framing on purpose. Echoing "everything after the blank line"
             * would make the Content-Length arm VACUOUS: a mutation that deleted the
             * request's Content-Length header left every arm green, because the body
             * happened to arrive in the same TLS record and got echoed anyway. A real
             * server frames by the declared length, so this one does too - and now
             * dropping the header turns arm F red, which is the point of the arm. */
            const char* cl = strstr(req, "Content-Length:");
            if (!cl) {
                static const char* const need = "no Content-Length on a body request\n";
                snprintf(head, sizeof(head), "HTTP/1.1 411 Length Required\r\n"
                                             "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                         strlen(need));
                if (ssl_write_all(&ssl, head, strlen(head)) != 0) goto done;
                if (ssl_write_all(&ssl, need, strlen(need)) != 0) goto done;
            } else {
                const char* rbody = sep + 4;
                size_t      have  = got - (size_t)(sep + 4 - req);
                size_t      want  = (size_t)strtoul(cl + 15, NULL, 10);
                size_t      rlen  = (want < have) ? want : have;
                snprintf(head, sizeof(head), "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n"
                                             "Connection: close\r\n\r\n", rlen);
                if (ssl_write_all(&ssl, head, strlen(head)) != 0) goto done;
                if (rlen && ssl_write_all(&ssl, rbody, rlen) != 0) goto done;
            }
        } else {
            static const char* const nf = "no such thing\n";
            snprintf(head, sizeof(head), "HTTP/1.1 404 Not Found\r\nContent-Length: %zu\r\n"
                                         "Connection: close\r\n\r\n", strlen(nf));
            if (ssl_write_all(&ssl, head, strlen(head)) != 0) goto done;
            if (ssl_write_all(&ssl, nf, strlen(nf)) != 0) goto done;
        }

        do { ret = mbedtls_ssl_close_notify(&ssl); }
        while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    }

done:
    free(big_chunk);
    free(big_body);
    mbedtls_net_free(&client);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&srvcrt);
    mbedtls_pk_free(&pkey);
    mbedtls_ctr_drbg_free(&rng);
    mbedtls_entropy_free(&entropy);
}

static void* server_main(void* arg)
{
    Server* s = (Server*)arg;
    mbedtls_net_set_nonblock(&s->listen_ctx);
    while (!s->stop) {
        mbedtls_net_context client;
        mbedtls_net_init(&client);
        int ret = mbedtls_net_accept(&s->listen_ctx, &client, NULL, 0, NULL);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbedtls_net_usleep(2000);
            continue;
        }
        if (ret != 0) { mbedtls_net_free(&client); continue; }
        mbedtls_net_set_block(&client);   /* macOS hands back a non-blocking socket */
        serve_one(&client, s->good);
    }
    return NULL;
}

static int listening_port(int fd)
{
    struct sockaddr_in addr;
    socklen_t          len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    if (getsockname(fd, (struct sockaddr*)&addr, &len) != 0) return -1;
    return (int)ntohs(addr.sin_port);
}

static int write_pem(const char* path, const char* pem)
{
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fputs(pem, f);
    fclose(f);
    return 0;
}

static void set_trust(const char* path)
{
#ifdef _WIN32
    _putenv_s("SSL_CERT_FILE", path);
#else
    setenv("SSL_CERT_FILE", path, 1);
#endif
}

/* ------------------------------------------------------------------ the arms */

int main(void)
{
    printf("=== native HTTPS transport (src/wyn_https.c) ===\n");

    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_rng);
    static const char* const pers = "wyn-https-test-pki";
    if (mbedtls_ctr_drbg_seed(&g_rng, mbedtls_entropy_func, &g_entropy,
                              (const unsigned char*)pers, strlen(pers)) != 0) {
        bad("seed the test RNG", "mbedtls_ctr_drbg_seed failed");
        return 1;
    }

    Ident ca, other_ca, good;
    if (ident_init(&ca, "Wyn HTTPS Test CA") != 0 || ident_issue(&ca, &ca, 1) != 0 ||
        ident_init(&other_ca, "Some Other CA") != 0 || ident_issue(&other_ca, &other_ca, 1) != 0 ||
        ident_init(&good, "127.0.0.1") != 0 || ident_issue(&good, &ca, 0) != 0) {
        bad("mint test certificates", "x509 write failed");
        return 1;
    }

    const char* tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = ".";
    char ca_path[512], other_path[512];
    snprintf(ca_path, sizeof(ca_path), "%s/wyn_https_test_ca.pem", tmp);
    snprintf(other_path, sizeof(other_path), "%s/wyn_https_test_other_ca.pem", tmp);
    if (write_pem(ca_path, ca.crt_pem) != 0 || write_pem(other_path, other_ca.crt_pem) != 0) {
        bad("write the test trust bundles", ca_path);
        return 1;
    }
    set_trust(ca_path);

    Server s;
    s.good = &good;
    s.stop = 0;
    mbedtls_net_init(&s.listen_ctx);
    if (mbedtls_net_bind(&s.listen_ctx, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP) != 0) {
        bad("bind loopback listener", "mbedtls_net_bind failed");
        return 1;
    }
    int port = listening_port(s.listen_ctx.fd);
    if (port <= 0) {
        bad("discover ephemeral port", "getsockname failed");
        return 1;
    }

    pthread_t th;
    if (pthread_create(&th, NULL, server_main, &s) != 0) {
        bad("start server thread", "pthread_create failed");
        return 1;
    }

    char base[128];
    snprintf(base, sizeof(base), "https://127.0.0.1:%d", port);
    char err[512];
    char url[256];

    /* Arm A - a 200 with Content-Length comes back as EXACTLY that body. */
    {
        WynHttpResponse r;
        snprintf(url, sizeof(url), "%s/small", base);
        if (wyn_https_request("GET", url, NULL, NULL, &r, err, sizeof(err)) != 0) {
            bad("GET a Content-Length body over native TLS", err);
        } else if (r.status != 200) {
            char d[64]; snprintf(d, sizeof(d), "status %d", r.status);
            bad("GET a Content-Length body: status 200", d);
            wyn_https_response_free(&r);
        } else if (r.body_len != strlen(SMALL_BODY) || memcmp(r.body, SMALL_BODY, r.body_len) != 0) {
            char d[256];
            snprintf(d, sizeof(d), "got %zu bytes: %.60s", r.body_len, r.body ? r.body : "(null)");
            bad("GET a Content-Length body: exact bytes", d);
            wyn_https_response_free(&r);
        } else {
            ok("a 200 with Content-Length returns exactly that body");
            wyn_https_response_free(&r);
        }
    }

    /* Arm B - CHUNKED is decoded. Three chunks, one of them 10 KB, one carrying a
     * chunk extension. The old code left the hex length lines in the body. */
    {
        char* bigc = make_big_chunk();
        size_t want_len = 6 + (size_t)BIG_CHUNK_LEN + 6;
        char*  want     = (char*)malloc(want_len + 1);
        memcpy(want, "alpha-", 6);
        memcpy(want + 6, bigc, BIG_CHUNK_LEN);
        memcpy(want + 6 + BIG_CHUNK_LEN, "-omega", 6);
        want[want_len] = '\0';

        WynHttpResponse r;
        snprintf(url, sizeof(url), "%s/chunked", base);
        if (wyn_https_request("GET", url, NULL, NULL, &r, err, sizeof(err)) != 0) {
            bad("GET a chunked body over native TLS", err);
        } else if (r.status != 200) {
            char d[64]; snprintf(d, sizeof(d), "status %d", r.status);
            bad("chunked: status 200", d);
            wyn_https_response_free(&r);
        } else if (r.body_len != want_len || memcmp(r.body, want, want_len) != 0) {
            char d[256];
            snprintf(d, sizeof(d), "expected %zu bytes, got %zu; head=%.24s",
                     want_len, r.body_len, r.body ? r.body : "(null)");
            bad("a chunked response decodes to the exact expected bytes", d);
            wyn_https_response_free(&r);
        } else {
            ok("a chunked response (3 chunks, one 10 KB) decodes to the exact bytes");
            wyn_https_response_free(&r);
        }
        free(bigc);
        free(want);
    }

    /* Arm C - the 128 KB cap regression test. 250 KB must arrive COMPLETE. */
    {
        char* want = make_big_body();
        WynHttpResponse r;
        snprintf(url, sizeof(url), "%s/big", base);
        if (wyn_https_request("GET", url, NULL, NULL, &r, err, sizeof(err)) != 0) {
            bad("GET a 250 KB body over native TLS", err);
        } else if (r.body_len != (size_t)BIG_BODY_LEN ||
                   memcmp(r.body, want, (size_t)BIG_BODY_LEN) != 0) {
            char d[160];
            snprintf(d, sizeof(d), "expected %d bytes, got %zu (the old cap was 131071)",
                     BIG_BODY_LEN, r.body_len);
            bad("a response larger than 200 KB comes back complete", d);
            wyn_https_response_free(&r);
        } else {
            ok("a 250 KB response comes back complete (no 128 KB cap)");
            wyn_https_response_free(&r);
        }
        free(want);
    }

    /* Arm D - a 404 is DISTINGUISHABLE from a 200. The old code discarded the
     * status line with the rest of the headers. */
    {
        WynHttpResponse r;
        snprintf(url, sizeof(url), "%s/nope", base);
        if (wyn_https_request("GET", url, NULL, NULL, &r, err, sizeof(err)) != 0) {
            bad("GET a 404 over native TLS", err);
        } else if (r.status != 404) {
            char d[96]; snprintf(d, sizeof(d), "status came back %d, not 404", r.status);
            bad("a 404 is distinguishable from a 200", d);
            wyn_https_response_free(&r);
        } else if (!r.body || strstr(r.body, "no such thing") == NULL) {
            bad("a 404 still yields its body", r.body ? r.body : "(null)");
            wyn_https_response_free(&r);
        } else {
            ok("a 404 is reported as 404 (status reachable) and keeps its body");
            wyn_https_response_free(&r);
        }
    }

    /* Arm F - a POST body is sent with a correct Content-Length, and the response
     * headers are available. The server echoes the request body back. */
    {
        static const char* const payload = "name=wyn&kind=native&len=deliberately-not-round";
        WynHttpResponse r;
        snprintf(url, sizeof(url), "%s/echo", base);
        if (wyn_https_request("POST", url, payload, NULL, &r, err, sizeof(err)) != 0) {
            bad("POST a body over native TLS", err);
        } else if (r.status != 200) {
            char d[96];
            snprintf(d, sizeof(d), "status %d (411 means no Content-Length was sent)",
                     r.status);
            bad("a POST body is sent with a Content-Length", d);
            wyn_https_response_free(&r);
        } else if (r.body_len != strlen(payload) ||
                   memcmp(r.body, payload, r.body_len) != 0) {
            char d[256];
            snprintf(d, sizeof(d), "echo mismatch: sent %zu, got %zu [%.60s]",
                     strlen(payload), r.body_len, r.body ? r.body : "(null)");
            bad("a POST body is sent verbatim with a correct Content-Length", d);
            wyn_https_response_free(&r);
        } else if (!r.headers || strstr(r.headers, "HTTP/1.1 200") == NULL) {
            bad("the response headers are captured", r.headers ? r.headers : "(null)");
            wyn_https_response_free(&r);
        } else {
            ok("a POST body round-trips and the response headers are captured");
            wyn_https_response_free(&r);
        }
    }

    /* Arm G - header injection. http_set_header(key, val) builds its line out of
     * caller data, so a bare LF inside a VALUE splits the request into two. A
     * well-formed multi-line block must still be accepted, or the check would be
     * refusing legitimate headers instead of injected ones - so both halves are
     * asserted here. This is the native analogue of the shell splice. */
    {
        WynHttpResponse r;
        snprintf(url, sizeof(url), "%s/small", base);
        if (wyn_https_request("GET", url, NULL, "X-Trace: a\nX-Evil: 1\r\n", &r,
                              err, sizeof(err)) == 0) {
            bad("a bare LF in a caller header value is REFUSED",
                "the request was sent anyway");
            wyn_https_response_free(&r);
        } else if (!strstr(err, "header")) {
            bad("a bare LF in a caller header is refused with a clear reason", err);
        } else if (wyn_https_request("GET", url, NULL, "X-A: 1\r\nX-B: 2\r\n", &r,
                                     err, sizeof(err)) != 0) {
            bad("a well-formed multi-header block is still ACCEPTED", err);
        } else {
            wyn_https_response_free(&r);
            ok("an injected header is refused while a well-formed block is accepted");
        }
    }

    /* Arm H - the URL cannot smuggle a second request line either. */
    {
        WynHttpResponse r;
        snprintf(url, sizeof(url), "%s/small\r\nX-Evil: 1", base);
        if (wyn_https_request("GET", url, NULL, NULL, &r, err, sizeof(err)) == 0) {
            bad("a CRLF in the URL is REFUSED", "the request was sent anyway");
            wyn_https_response_free(&r);
        } else if (!strstr(err, "url") && !strstr(err, "URL")) {
            bad("a CRLF in the URL is refused with a clear reason", err);
        } else {
            ok("a CRLF in the URL is refused");
        }
    }

    /* Arm E - an untrusted certificate is REJECTED. The seam guarantees this; the
     * point of the arm is that the integration did not undo it (by inventing a
     * skip-verify option, or by passing trust material the seam then ignores). */
    {
        set_trust(other_path);
        WynHttpResponse r;
        snprintf(url, sizeof(url), "%s/small", base);
        if (wyn_https_request("GET", url, NULL, NULL, &r, err, sizeof(err)) == 0) {
            bad("an untrusted certificate is REJECTED",
                "the request succeeded - verification is not happening");
            wyn_https_response_free(&r);
        } else if (!strstr(err, "verification failed")) {
            /* Distinguish from "the connection failed for some other reason", or the
             * arm passes for the wrong cause and would keep passing with no TLS at all. */
            bad("an untrusted certificate is rejected for the RIGHT reason", err);
        } else {
            ok("an untrusted certificate is rejected");
        }
        set_trust(ca_path);
    }

    /* The wrong-NAME case (valid chain, certificate issued for another host) is
     * gated one layer down, in tests/tls/test_tls_seam.c arm 4, because doing it
     * here would need a second name that resolves to this listener - "localhost"
     * usually does, but on a runner where it resolves to ::1 only, the arm would
     * fail as "connect failed" and read as a verification regression. A flaky
     * merge gate is worse than the same property checked once, deterministically. */

    s.stop = 1;
    pthread_join(th, NULL);
    mbedtls_net_free(&s.listen_ctx);
    ident_free(&ca); ident_free(&other_ca); ident_free(&good);
    mbedtls_ctr_drbg_free(&g_rng);
    mbedtls_entropy_free(&g_entropy);
    remove(ca_path);
    remove(other_path);

    if (failures) printf("=== native HTTPS: %d of %d checks FAILING ===\n", failures, checks);
    else          printf("=== native HTTPS: all %d checks pass ===\n", checks);
    return failures ? 1 : 0;
}
