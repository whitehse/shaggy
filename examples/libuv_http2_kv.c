/*
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <getopt.h>
#include <fcntl.h>
#include "http2.h"

static int listen_port = 8080;
static char listen_addr[64] = "0.0.0.0";
static int enable_tls = 0;
static char key_file[256] = "";
static char cert_file[256] = "";

static void parse_args(int argc, char **argv) {
    static struct option long_opts[] = {{"port",1,0,'p'},{"address",1,0,'a'},{"tls",0,0,'t'},{"key",1,0,'k'},{"cert",1,0,'c'},{0,0,0,0}};
    int opt; while ((opt=getopt_long(argc,argv,"p:a:tk:c:",long_opts,NULL))!=-1) {
        if(opt=='p')listen_port=atoi(optarg);
        else if(opt=='a')strncpy(listen_addr,optarg,sizeof(listen_addr)-1);
        else if(opt=='t')enable_tls=1;
        else if(opt=='k')strncpy(key_file,optarg,sizeof(key_file)-1);
        else if(opt=='c')strncpy(cert_file,optarg,sizeof(cert_file)-1);
    }
}

#define MAX_CONNS 256
struct conn { int fd; http2_ctx_t *ctx; uint8_t buf[8192]; uv_tcp_t *handle; } conns[MAX_CONNS];
int num_conns = 0;

static void handle_request(struct conn *c, size_t len) {
    if (memmem(c->buf, len, "index.html", 10))
        printf("hello world /index.html (libuv fd=%d)\n", c->fd);
    http2_feed_input(c->ctx, c->buf, len);
}

static void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    (void)handle;
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    for (int j = 0; j < num_conns; j++) {
        if (conns[j].handle == (uv_tcp_t*)stream) {
            if (nread > 0) {
                memcpy(conns[j].buf, buf->base, nread);
                handle_request(&conns[j], nread);
            }
            free(buf->base);
            break;
        }
    }
}

static void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0 || num_conns >= MAX_CONNS) return;
    uv_tcp_t *client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);
    if (uv_accept(server, (uv_stream_t*)client) == 0) {
        conns[num_conns].fd = client->io_watcher.fd;
        conns[num_conns].ctx = http2_create_with_role(HTTP2_ROLE_SERVER);
        conns[num_conns].handle = client;
        uv_read_start((uv_stream_t*)client, alloc_cb, on_read);
        num_conns++;
    } else {
        uv_close((uv_handle_t*)client, (uv_close_cb)free);
    }
}

int main(int argc, char **argv) {
    parse_args(argc, argv);
    if (enable_tls) printf("kTLS (libuv) key=%s\n", key_file);
    printf("libuv multi-conn listening %s:%d\n", listen_addr, listen_port);

    uv_loop_t *loop = uv_default_loop();
    uv_tcp_t server;
    uv_tcp_init(loop, &server);
    struct sockaddr_in addr;
    uv_ip4_addr(listen_addr, listen_port, &addr);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    uv_listen((uv_stream_t*)&server, 128, on_new_connection);
    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}