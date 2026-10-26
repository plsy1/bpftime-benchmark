// sslsniff target: an OpenSSL client and server in one thread, joined by a BIO pair (no
// sockets). After the handshake the client SSL_writes a fixed message and the server SSL_reads
// it, for DURATION seconds. Prints the median round trips/s and the exact count; each round trip
// is one SSL_write and one SSL_read (sslsniff reports both). Optional PAUSE_US sleeps after
// every 100 round trips, to keep the tool's per-event output small in functional audits.
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static int cmp(const void *a, const void *b) { double x = *(double *)a, y = *(double *)b; return (x > y) - (x < y); }
static void die(const char *what) { fprintf(stderr, "%s failed\n", what); ERR_print_errors_fp(stderr); exit(1); }

static void self_signed(SSL_CTX *ctx) {
    EVP_PKEY *key = EVP_EC_gen("P-256");
    X509 *x = X509_new();
    if (!key || !x) die("key");
    ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
    X509_gmtime_adj(X509_getm_notBefore(x), 0);
    X509_gmtime_adj(X509_getm_notAfter(x), 3600);
    X509_set_pubkey(x, key);
    X509_NAME_add_entry_by_txt(X509_get_subject_name(x), "CN", MBSTRING_ASC, (unsigned char *)"localhost", -1, -1, 0);
    X509_set_issuer_name(x, X509_get_subject_name(x));
    if (!X509_sign(x, key, EVP_sha256())) die("X509_sign");
    if (SSL_CTX_use_certificate(ctx, x) != 1 || SSL_CTX_use_PrivateKey(ctx, key) != 1) die("use cert");
    X509_free(x);
    EVP_PKEY_free(key);
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 10;
    int pause_us = argc > 2 ? atoi(argv[2]) : 0;
    SSL_CTX *sctx = SSL_CTX_new(TLS_server_method()), *cctx = SSL_CTX_new(TLS_client_method());
    if (!sctx || !cctx) die("SSL_CTX_new");
    self_signed(sctx);
    SSL *srv = SSL_new(sctx), *cli = SSL_new(cctx);
    BIO *sb, *cb;
    if (!BIO_new_bio_pair(&sb, 1 << 16, &cb, 1 << 16)) die("BIO_new_bio_pair");
    SSL_set_bio(srv, sb, sb);
    SSL_set_bio(cli, cb, cb);
    SSL_set_accept_state(srv);
    SSL_set_connect_state(cli);
    for (int i = 0; i < 100; i++) {   // drive both sides until the handshake completes
        int c = SSL_do_handshake(cli), s = SSL_do_handshake(srv);
        if (c == 1 && s == 1) break;
        if (i == 99) die("handshake");
    }
    const char msg[] = "sslloop-message-0123456789abcdef";
    char buf[256];
    double rates[1000]; int k = 0; unsigned long ops = 0, total_ops = 0;
    double mark = now();
    while (k < duration) {
        for (int j = 0; j < 100; j++) {
            if (SSL_write(cli, msg, sizeof msg) != (int)sizeof msg) die("SSL_write");
            if (SSL_read(srv, buf, sizeof buf) != (int)sizeof msg) die("SSL_read");
        }
        ops += 100; total_ops += 100;
        if (pause_us) usleep(pause_us);
        double t = now();
        if (t - mark >= 1.0) { rates[k++] = ops / (t - mark); ops = 0; mark = t; }
    }
    qsort(rates, k, sizeof(double), cmp);
    printf("ops_per_sec_median %.1f\n", rates[k / 2]);
    printf("total_ops %lu\n", total_ops);
    return 0;
}
