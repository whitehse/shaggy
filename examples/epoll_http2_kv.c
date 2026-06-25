#define _GNU_SOURCE
/*
 * epoll_http2_kv.c - Multi-connection HTTP/2 example with OpenSSL + Linux kTLS
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <fcntl.h>
#include <linux/tls.h>
#include "http2.h"

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#endif

#define MAX_EVENTS 64
#define BUF_SIZE 8192
#define MAX_CONNS 1024

static int listen_port = 8080;
static char listen_addr[64] = "0.0.0.0";
static int enable_tls = 0;
static char key_file[256] = "";
static char cert_file[256] = "";

#ifdef HAVE_OPENSSL
static SSL_CTX *ssl_ctx = NULL;
#endif

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  --port PORT        Listen port (default 8080)\n"
        "  --address ADDR     Listen address (default 0.0.0.0)\n"
        "  --tls              Enable TLS (requires --key and --cert)\n"
        "  --key KEYFILE      Private key file (PEM)\n"
        "  --cert CERTFILE    Certificate file (PEM)\n", prog);
}

static void parse_args(int argc, char **argv) {
    static struct option long_opts[] = {
        {"port", required_argument, 0, 'p'},
        {"address", required_argument, 0, 'a'},
        {"tls", no_argument, 0, 't'},
        {"key", required_argument, 0, 'k'},
        {"cert", required_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "p:a:tk:c:h", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'p': listen_port = atoi(optarg); break;
            case 'a': strncpy(listen_addr, optarg, sizeof(listen_addr)-1); break;
            case 't': enable_tls = 1; break;
            case 'k': strncpy(key_file, optarg, sizeof(key_file)-1); break;
            case 'c': strncpy(cert_file, optarg, sizeof(cert_file)-1); break;
            case 'h': print_usage(argv[0]); exit(0);
            default: print_usage(argv[0]); exit(1);
        }
    }
    if (enable_tls && (!key_file[0] || !cert_file[0])) {
        fprintf(stderr, "Error: --tls requires --key and --cert\n");
        exit(1);
    }
}

#ifdef HAVE_OPENSSL
static void init_openssl(void) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file, SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    /* ALPN for HTTP/2 */
    static const unsigned char alpn[] = { 2, 'h', '2' };
    SSL_CTX_set_alpn_protos(ssl_ctx, alpn, sizeof(alpn));
}

static int try_enable_ktls(int fd) {
    /* Attempt Linux kTLS offload (best effort) */
    struct tls12_crypto_info_aes_gcm_128 crypto_info;
    memset(&crypto_info, 0, sizeof(crypto_info));
    crypto_info.info.version = TLS_1_2_VERSION;
    crypto_info.info.cipher_type = TLS_CIPHER_AES_GCM_128;
    /* In a real implementation you would extract the keys from the SSL session here */
    if (setsockopt(fd, SOL_TLS, TLS_TX, &crypto_info, sizeof(crypto_info)) < 0) {
        /* kTLS not available or keys not extracted - fall back to user-space TLS */
        return 0;
    }
    return 1;
}
#endif

struct conn {
    int fd;
    http2_ctx_t *ctx;
#ifdef HAVE_OPENSSL
    SSL *ssl;
    int ktls_enabled;
#endif
    uint8_t buf[BUF_SIZE];
};

static struct conn conns[MAX_CONNS];
static int num_conns = 0;

static void handle_request(struct conn *c, size_t len) {
    if (memmem(c->buf, len, "index.html", 10)) {
        printf("Serving hello world for /index.html on fd %d\n", c->fd);
    }
    http2_feed_input(c->ctx, c->buf, len);
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

#ifdef HAVE_OPENSSL
    if (enable_tls) {
        init_openssl();
        printf("TLS enabled with OpenSSL + kTLS support (cert=%s)\n", cert_file);
    }
#endif

    printf("Listening on %s:%d (multi-conn epoll + TLS)\n", listen_addr, listen_port);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(listen_port) };
    inet_pton(AF_INET, listen_addr, &addr.sin_addr);
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    int epfd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = listen_fd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == listen_fd) {
                int client = accept(listen_fd, NULL, NULL);
                if (client >= 0 && num_conns < MAX_CONNS) {
                    fcntl(client, F_SETFL, O_NONBLOCK);
                    conns[num_conns].fd = client;
                    conns[num_conns].ctx = http2_create_with_role(HTTP2_ROLE_SERVER);
#ifdef HAVE_OPENSSL
                    if (enable_tls && ssl_ctx) {
                        conns[num_conns].ssl = SSL_new(ssl_ctx);
                        SSL_set_fd(conns[num_conns].ssl, client);
                        if (SSL_accept(conns[num_conns].ssl) <= 0) {
                            ERR_print_errors_fp(stderr);
                            close(client);
                            continue;
                        }
                        conns[num_conns].ktls_enabled = try_enable_ktls(client);
                    }
#endif
                    struct epoll_event cev = { .events = EPOLLIN, .data.fd = client };
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client, &cev);
                    num_conns++;
                }
            } else {
                for (int j = 0; j < num_conns; j++) {
                    if (conns[j].fd == events[i].data.fd) {
                        ssize_t r = 0;
#ifdef HAVE_OPENSSL
                        if (conns[j].ssl) {
                            r = SSL_read(conns[j].ssl, conns[j].buf, BUF_SIZE);
                        } else
#endif
                        {
                            r = read(conns[j].fd, conns[j].buf, BUF_SIZE);
                        }
                        if (r > 0) {
                            handle_request(&conns[j], r);
                        } else if (r <= 0) {
                            close(conns[j].fd);
#ifdef HAVE_OPENSSL
                            if (conns[j].ssl) SSL_free(conns[j].ssl);
#endif
                            http2_destroy(conns[j].ctx);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}