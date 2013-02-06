/*
 *  twemperf - a tool for measuring memcached server performance.
 *  Copyright (C) 2011 Twitter, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MCP_CONN_H_
#define _MCP_CONN_H_

#include <mcp_generator.h>

struct conn {
    STAILQ_ENTRY(conn) conn_tqe;            /* link in free q */
    uint64_t           id;                  /* unique id */
    struct context     *ctx;                /* owner context */

    uint32_t           ncall_sendq;         /* # call send q */
    struct call_tqh    call_sendq;          /* call send q */
    uint32_t           ncall_recvq;         /* # call recv q */
    struct call_tqh    call_recvq;          /* call recv q */

    struct timer       *watchdog;           /* connection watchdog timer */
    double             connect_start;       /* connect start in sec */

    int                sd;                  /* socket descriptor */

    char               buf[8 * KB];         /* conn buffer */

    struct gen         call_gen;            /* call generator */
    uint32_t           ncall_created;       /* # call created */
    uint32_t           ncall_create_failed; /* # call create failed */
    uint32_t           ncall_completed;     /* # call completed */

    err_t              err;                 /* connection errno? */
    unsigned           recv_active:1;       /* recv active? */
    unsigned           recv_ready:1;        /* recv ready? */
    unsigned           send_active:1;       /* send active? */
    unsigned           send_ready:1;        /* send ready? */

    unsigned           connecting:1;        /* connecting? */
    unsigned           connected:1;         /* connected? */
    unsigned           eof:1;               /* eof? */
};

STAILQ_HEAD(conn_tqh, conn);

struct conn *conn_get(struct context *ctx);
void conn_put(struct conn *conn);

ssize_t conn_sendv(struct conn *conn, struct iovec *iov, int iovcnt, size_t iov_size);
ssize_t conn_recv(struct conn *conn, void *buf, size_t size);

void conn_init(void);
void conn_deinit(void);

#endif
