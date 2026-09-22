/* test_tls_seam.c - gate for src/wyn_tls.c.
 *
 * Runs a real TLS server on 127.0.0.1 inside this process and drives the client
 * seam against it. Self-contained by design:
 *
 *   - the CA and the leaf certificates are MINTED AT RUNTIME by mbedTLS, so no
 *     private key is ever committed (a hard-coded key in a test file is
 *     indistinguishable, to a scanner and to a reader, from a leaked one) and
 *     nothing expires on a future maintainer;
 *   - the port comes from the OS, so the test cannot collide with whatever else
 *     is listening;
 *   - loopback only: no network egress, and no openssl CLI - which is the whole
 *     point of src/wyn_tls.c.
 *
 * The three failure arms matter more than the happy path. The classic TLS-client
 * bug is not "handshake broken", it is "handshake succeeds when it must not":
 * trusting an unrelated CA, accepting a certificate issued for a different name,
 * or connecting with verification effectively off. One arm each.
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

#include "wyn_tls.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509_csr.h"

static int failures = 0;

static void ok(const char* name) { printf("  PASS: %s\n", name); }
static void bad(const char* name, const char* detail)
{
    printf("  FAIL: %s\n        %s\n", name, detail ? detail : "");
    failures++;
}

/* ------------------------------------------------- certificates, minted here */

typedef struct {
    mbedtls_pk_context key;
    char               crt_pem[4096];
    char               key_pem[4096];
    char               dn[128];        /* "CN=..." */
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

/* Issue id's certificate, signed by issuer (or by itself when issuer == id).
 * No subjectAltName on purpose: with no SAN, the hostname check falls back to the
 * CN, which keeps this test's trust material as small as the property it tests. */
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
    /* Fixed, deliberately wide validity: an expiring test certificate is a test
     * that breaks on a calendar date for no reason. */
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

/* ---------------------------------------------------------------- the server */

typedef struct {
    mbedtls_net_context listen_ctx;
    const Ident*        good;   /* CN=127.0.0.1 - matches what the client asks for */
    const Ident*        wrong;  /* valid chain, CN=wyn-test.invalid */
    volatile int        stop;   /* set by main when the arms are done */
} Server;

/* Serve one connection with the given identity. Handshake failures are expected
 * on some arms, so they are swallowed rather than reported. */
static void serve_one(mbedtls_net_context* client_in, const Ident* id)
{
    mbedtls_net_context      client = *client_in;
    mbedtls_ssl_context      ssl;
    mbedtls_ssl_config       conf;
    mbedtls_x509_crt         srvcrt;
    mbedtls_pk_context       pkey;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context rng;

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&srvcrt);
    mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&rng);

    static const char* const pers = "wyn-tls-test-server";
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
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);   /* no client certs here */
    if (mbedtls_ssl_conf_own_cert(&conf, &srvcrt, &pkey) != 0) goto done;
    if (mbedtls_ssl_setup(&ssl, &conf) != 0) goto done;

    mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, NULL);

    int ret;
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) goto done;
    }

    /* Echo protocol: read one request, answer "pong\n". */
    unsigned char buf[256];
    do { ret = mbedtls_ssl_read(&ssl, buf, sizeof(buf) - 1); }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (ret > 0) {
        const char*          reply = "pong\n";
        size_t               left  = strlen(reply);
        const unsigned char* p     = (const unsigned char*)reply;
        while (left > 0) {
            ret = mbedtls_ssl_write(&ssl, p, left);
            if (ret > 0) { p += ret; left -= (size_t)ret; continue; }
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) break;
        }
    }
    do { ret = mbedtls_ssl_close_notify(&ssl); }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

done:
    mbedtls_net_free(&client);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&srvcrt);
    mbedtls_pk_free(&pkey);
    mbedtls_ctr_drbg_free(&rng);
    mbedtls_entropy_free(&entropy);
}

/* Serve whatever connects until main says stop.
 *
 * Deliberately NOT a fixed count of accepts. With a fixed count, an arm that
 * correctly refuses to open a socket leaves the server blocked in accept() and the
 * whole test dies on the watchdog - which is how a mutation of the fail-closed path
 * first showed up here: as a 60s hang, indistinguishable from infrastructure
 * trouble, instead of as a named failing arm. */
static void* server_main(void* arg)
{
    Server* s = (Server*)arg;
    int     n = 0;
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
        /* Connection 3 gets the valid-chain, wrong-name identity - arm 4's point.
         * Everything else gets the matching one. */
        serve_one(&client, (++n == 3) ? s->wrong : s->good);
    }
    return NULL;
}

/* Ask the OS which port it gave us, so the test never picks one. */
static int listening_port(int fd)
{
    struct sockaddr_in addr;
    socklen_t          len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    if (getsockname(fd, (struct sockaddr*)&addr, &len) != 0) return -1;
    return (int)ntohs(addr.sin_port);
}

/* ------------------------------------------------------------------ the arms
 *
 * The arms run in order and consume the server's connections in order, so arm 3
 * passing depends on the seam refusing BEFORE it opens a socket. That is
 * deliberate: when the fail-closed check was mutated out, arm 3 took connection
 * three and arm 4 was left waiting for a server that had finished - both arms
 * went red. A silent shift of one connection is exactly the kind of thing this
 * ordering makes loud.
 */

int main(void)
{
    printf("=== TLS seam (src/wyn_tls.c) ===\n");

    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_rng);
    static const char* const pers = "wyn-tls-test-pki";
    if (mbedtls_ctr_drbg_seed(&g_rng, mbedtls_entropy_func, &g_entropy,
                              (const unsigned char*)pers, strlen(pers)) != 0) {
        bad("seed the test RNG", "mbedtls_ctr_drbg_seed failed");
        return 1;
    }

    Ident ca, other_ca, good, wrong;
    if (ident_init(&ca, "Wyn TLS Seam Test CA") != 0 || ident_issue(&ca, &ca, 1) != 0 ||
        ident_init(&other_ca, "Some Other CA") != 0 || ident_issue(&other_ca, &other_ca, 1) != 0 ||
        ident_init(&good, "127.0.0.1") != 0 || ident_issue(&good, &ca, 0) != 0 ||
        ident_init(&wrong, "wyn-test.invalid") != 0 || ident_issue(&wrong, &ca, 0) != 0) {
        bad("mint test certificates", "x509 write failed");
        return 1;
    }

    Server s;
    s.good  = &good;
    s.wrong = &wrong;
    s.stop  = 0;
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
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    pthread_t th;
    if (pthread_create(&th, NULL, server_main, &s) != 0) {
        bad("start server thread", "pthread_create failed");
        return 1;
    }

    char err[512];

    /* Arm 1 - the happy path: trusted CA, matching name, bytes both ways. */
    {
        WynTlsOptions opt = { NULL, ca.crt_pem, 0, 5000 };
        WynTls*       t   = wyn_tls_connect("127.0.0.1", port_str, &opt, err, sizeof(err));
        if (!t) {
            bad("handshake with a trusted CA succeeds", err);
        } else {
            const char* req = "ping\n";
            char        buf[64] = {0};
            long        w = wyn_tls_write(t, req, strlen(req));
            long        r = (w == (long)strlen(req)) ? wyn_tls_read(t, buf, sizeof(buf) - 1) : -1;
            if (w != (long)strlen(req))                       bad("write over TLS", wyn_tls_error(t));
            else if (r <= 0 || strncmp(buf, "pong\n", 5) != 0) bad("read back over TLS", buf);
            else ok("handshake, write and read against a trusted server");
            wyn_tls_close(t);
        }
    }

    /* Arm 2 - an unrelated CA must NOT verify. */
    {
        WynTlsOptions opt = { NULL, other_ca.crt_pem, 0, 5000 };
        WynTls*       t   = wyn_tls_connect("127.0.0.1", port_str, &opt, err, sizeof(err));
        if (t) {
            bad("an untrusted certificate is REJECTED",
                "handshake succeeded - verification is not happening");
            wyn_tls_close(t);
        } else if (!strstr(err, "verification failed")) {
            bad("an untrusted certificate is rejected for the right reason", err);
        } else {
            ok("an untrusted certificate is rejected");
        }
    }

    /* Arm 3 - no trust anchors: fail closed, and do not even open a socket. */
    {
        WynTlsOptions opt = { NULL, NULL, 0, 5000 };
        WynTls*       t   = wyn_tls_connect("127.0.0.1", port_str, &opt, err, sizeof(err));
        if (t) {
            bad("connecting without trust anchors is REFUSED", "it connected");
            wyn_tls_close(t);
        } else if (!strstr(err, "no trust anchors")) {
            bad("connecting without trust anchors is refused with a clear reason", err);
        } else {
            ok("connecting without trust anchors is refused");
        }
    }

    /* Arm 4 - valid chain, wrong name. Only the hostname check can catch this. */
    {
        WynTlsOptions opt = { NULL, ca.crt_pem, 0, 5000 };
        WynTls*       t   = wyn_tls_connect("127.0.0.1", port_str, &opt, err, sizeof(err));
        if (t) {
            bad("a certificate issued for another name is REJECTED",
                "handshake succeeded - the hostname is not being checked");
            wyn_tls_close(t);
        } else if (!strstr(err, "verification failed")) {
            bad("a wrong-name certificate is rejected for the right reason", err);
        } else {
            ok("a certificate issued for another name is rejected");
        }
    }

    /* Arm 5 - the platform trust store yields roots HERE. A store that loads zero
     * roots is the failure mode that makes every later handshake inexplicable, so
     * it gets its own check with its own message. */
    {
        long n = wyn_tls_system_trust_count(err, sizeof(err));
        if (n < 20) {
            char detail[640];
            snprintf(detail, sizeof(detail), "found %ld roots%s%s", n, n < 0 ? ": " : "",
                     n < 0 ? err : "");
            bad("the platform trust store yields root certificates", detail);
        } else {
            char detail[64];
            snprintf(detail, sizeof(detail), "%ld roots", n);
            ok("the platform trust store yields root certificates");
            printf("        (%s)\n", detail);
        }
    }

    /* Arm 6 - system trust must not become "trust anything". The test CA is not a
     * public root, so a connection that trusts ONLY the system store must fail. */
    {
        WynTlsOptions opt = { NULL, NULL, 1, 5000 };
        WynTls*       t   = wyn_tls_connect("127.0.0.1", port_str, &opt, err, sizeof(err));
        if (t) {
            bad("system trust alone does NOT accept a private CA",
                "handshake succeeded against a certificate no public root signed");
            wyn_tls_close(t);
        } else if (!strstr(err, "verification failed")) {
            /* A machine with no roots at all would also fail here, for an unrelated
             * reason - distinguish, or this arm passes vacuously. */
            bad("system trust rejects a private CA for the right reason", err);
        } else {
            ok("system trust alone does not accept a private CA");
        }
    }

    /* Arm 7 - an explicit-but-empty trust source must produce a CLEAR error, not a
     * confusing one. This is the arm that pins crt_count's empty-head-node handling:
     * miscount an empty chain as 1 and the "no roots" guard never fires, so the user
     * sees "certificate verification failed" when the truth is "your bundle has no
     * certificates in it". SSL_CERT_FILE is authoritative, so no platform fallback
     * can mask it. */
    {
        const char* tmp = getenv("TMPDIR");
        char        empty_pem[512];
        snprintf(empty_pem, sizeof(empty_pem), "%s/wyn_tls_empty_roots.pem",
                 (tmp && *tmp) ? tmp : ".");
        FILE* f = fopen(empty_pem, "w");
        if (!f) {
            bad("create an empty trust bundle", empty_pem);
        } else {
            fputs("# no certificates here\n", f);
            fclose(f);
#ifdef _WIN32
            _putenv_s("SSL_CERT_FILE", empty_pem);
#else
            setenv("SSL_CERT_FILE", empty_pem, 1);
#endif
            long n = wyn_tls_system_trust_count(err, sizeof(err));
            if (n != -1) {
                char detail[128];
                snprintf(detail, sizeof(detail), "reported %ld roots from an empty bundle", n);
                bad("an empty trust bundle reports no roots", detail);
            } else {
                WynTlsOptions opt = { NULL, NULL, 1, 5000 };
                WynTls*       t   = wyn_tls_connect("127.0.0.1", port_str, &opt, err, sizeof(err));
                if (t) {
                    bad("an empty trust bundle REFUSES the connection", "it connected");
                    wyn_tls_close(t);
                } else if (!strstr(err, "no system root certificates")) {
                    bad("an empty trust bundle explains itself instead of failing verification", err);
                } else {
                    ok("an empty trust bundle refuses with a clear reason");
                }
            }
#ifdef _WIN32
            _putenv_s("SSL_CERT_FILE", "");
#else
            unsetenv("SSL_CERT_FILE");
#endif
            remove(empty_pem);
        }
    }

    s.stop = 1;
    pthread_join(th, NULL);
    mbedtls_net_free(&s.listen_ctx);
    ident_free(&ca); ident_free(&other_ca); ident_free(&good); ident_free(&wrong);
    mbedtls_ctr_drbg_free(&g_rng);
    mbedtls_entropy_free(&g_entropy);

    printf(failures ? "=== TLS seam: %d FAILING ===\n" : "=== TLS seam: all %d checks pass ===\n",
           failures ? failures : 7);
    return failures ? 1 : 0;
}
