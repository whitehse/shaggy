#define _GNU_SOURCE
/*
 * SPDX-License-Identifier: CC0-1.0
 */
#include <ev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <getopt.h>
#include "http2.h"

static int listen_port = 8080;
static char listen_addr[64] = "0.0.0.0";
static int enable_tls = 0;
static char key_file[256] = "";
static char cert_file[256] = "";

static void parse_args(int argc, char **argv) {
    /* same getopt as epoll example - abbreviated for space */
    static struct option long_opts[] = {{"port",1,0,'p'},{"address",1,0,'a'},{"tls",0,0,'t'},{"key",1,0,'k'},{"cert",1,0,'c'},{0,0,0,0}};
    int opt; while ((opt=getopt_long(argc,argv,"p:a:tk:c:",long_opts,NULL))!=-1) {
        if (opt=='p') listen_port=atoi(optarg);
        else if (opt=='a') strncpy(listen_addr,optarg,sizeof(listen_addr)-1);
        else if (opt=='t') enable_tls=1;
        else if (opt=='k') strncpy(key_file,optarg,sizeof(key_file)-1);
        else if (opt=='c') strncpy(cert_file,optarg,sizeof(cert_file)-1);
    }
}

#define MAX_CONNS 256
struct conn { int fd; http2_ctx_t *ctx; uint8_t buf[8192]; } conns[MAX_CONNS];
int num_conns = 0;
struct ev_loop *loop;

static void handle_request(struct conn *c, size_t len) {
    if (memmem(c->buf, len, "index.html", 10))
        printf("hello world for /index.html (libev, fd=%d)\n", c->fd);
    http2_feed_input(c->ctx, c->buf, len);
}

static void client_cb(EV_P_ ev_io *w, int revents) {
    (void)revents;
    for (int j=0; j<num_conns; j++) if (conns[j].fd == w->fd) {
        ssize_t r = read(w->fd, conns[j].buf, sizeof(conns[j].buf));
        if (r > 0) handle_request(&conns[j], r);
        break;
    }
}

static void accept_cb(EV_P_ ev_io *w, int revents) {
    (void)revents;
    int client = accept(w->fd, NULL, NULL);
    if (client >= 0 && num_conns < MAX_CONNS) {
        fcntl(client, F_SETFL, O_NONBLOCK);
        conns[num_conns].fd = client;
        conns[num_conns].ctx = http2_create_with_role(HTTP2_ROLE_SERVER);
        static ev_io cw[MAX_CONNS];
        ev_io_init(&cw[num_conns], client_cb, client, EV_READ);
        ev_io_start(loop, &cw[num_conns]);
        num_conns++;
    }
}

int main(int argc, char **argv) {
    parse_args(argc, argv);
    if (enable_tls) printf("kTLS enabled (libev) key=%s\n", key_file);
    printf("libev multi-conn on %s:%d\n", listen_addr, listen_port);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {.sin_family=AF_INET, .sin_port=htons(listen_port)};
    inet_pton(AF_INET, listen_addr, &a.sin_addr);
    bind(lfd, (struct sockaddr*)&a, sizeof(a)); listen(lfd, 128); fcntl(lfd,F_SETFL,O_NONBLOCK);

    loop = EV_DEFAULT;
    static ev_io accept_w;
    ev_io_init(&accept_w, accept_cb, lfd, EV_READ);
    ev_io_start(loop, &accept_w);
    ev_run(loop, 0);
    return 0;
}