/*
 * eco socket (eco+socket+eloop) - easy coroutine socket implementation
 *
 * Copyright (C) 2023-2024 huang <https://github.com/huangyajie>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ECO_SOCKET_H_
#define _ECO_SOCKET_H_

#ifdef __cpluscplus
extern "C" {
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include "eco.h"
#include "eloop.h"

// int socket(int domain, int type, int protocol)
int eco_socket(int domain, int type, int protocol);

// int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int eco_bind(int fd, const struct sockaddr *addr, socklen_t addrlen);

//int listen(int fd,int backlog);
int eco_listen(int fd,int backlog);

// int accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
int eco_accept(int fd, struct sockaddr *addr, socklen_t *addrlen);

// int connect(int fd, const struct sockaddr *address, socklen_t address_len)
int eco_connect(int fd, const struct sockaddr *address, socklen_t address_len);


// ssize_t read( int fd, void *buf, size_t nbyte )
ssize_t eco_read(int fd, void *buf, size_t nbyte);


// ssize_t write( int fd, const void *buf, size_t nbyte )
ssize_t eco_write(int fd, const void *buf, size_t nbyte);

// int close(int fd)
int eco_close(int fd);

int eco_poll(struct poll_fd* in,unsigned int nfds,struct poll_fd* out,unsigned int out_sz,int timeout);

unsigned int eco_sleep(unsigned int seconds);

unsigned int eco_msleep(unsigned int  msecs);

int eco_resume_later(struct schedule* sch,int id);

struct schedule* eco_get_cur_schedule();

int eco_loop_init();

int eco_loop_run();

int eco_loop_exit();





#ifdef __cpluscplus
}
#endif

#endif