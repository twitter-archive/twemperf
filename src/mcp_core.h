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

#ifndef _MCP_CORE_H_
#define _MCP_CORE_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_DEBUG_LOG
# define MCP_DEBUG_LOG 1
#endif

#ifdef HAVE_ASSERT_PANIC
# define MCP_ASSERT_PANIC 1
#endif

#define MCP_OK       0
#define MCP_ERROR   -1
#define MCP_EAGAIN  -2
#define MCP_ENOMEM  -3

typedef int rstatus_t; /* return type */
typedef int err_t;     /* error type */

struct context;
struct conn;
struct call;
struct epoll_event;
struct string;

typedef enum event_type {
    EVENT_INVALID           =  0,

    EVENT_CONN_CREATED      =  1,   /* connection events */
    EVENT_CONN_CONNECTING   =  2,
    EVENT_CONN_CONNECTED    =  3,
    EVENT_CONN_CLOSE        =  4,
    EVENT_CONN_TIMEOUT      =  5,
    EVENT_CONN_FAILED       =  6,
    EVENT_CONN_DESTROYED    =  7,

    EVENT_CALL_CREATED      =  8,   /* call events */
    EVENT_CALL_ISSUE_START  =  9,
    EVENT_CALL_SEND_START   = 10,
    EVENT_CALL_SEND_STOP    = 11,
    EVENT_CALL_RECV_START   = 12,
    EVENT_CALL_RECV_STOP    = 13,
    EVENT_CALL_DESTROYED    = 14,

    EVENT_GEN_CONN_TRIGGER  = 15,   /* generator trigger and fire events */
    EVENT_GEN_CONN_FIRE     = 16,
    EVENT_GEN_CALL_TRIGGER  = 17,
    EVENT_GEN_CALL_FIRE     = 18,
    EVENT_GEN_SIZE_TRIGGER  = 19,
    EVENT_GEN_SIZE_FIRE     = 20,

    MAX_EVENT_TYPES         = 21
} event_type_t;

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <mcp_queue.h>
#include <mcp_log.h>
#include <mcp_util.h>
#include <mcp_event.h>
#include <mcp_ecb.h>
#include <mcp_distribution.h>
#include <mcp_call.h>
#include <mcp_conn.h>
#include <mcp_timer.h>
#include <mcp_stats.h>
#include <mcp_generator.h>

struct string {
    char   *data; /* string length */
    size_t len;   /* string data */
};

struct opt {
    int               log_level;         /* log level */
    char              *log_filename;     /* log filename */

    char              *server;           /* server name */
    uint16_t          port;              /* server port */
    struct sockinfo   si;                /* server socket info */

    double            timeout;           /* connection timeout in sec */
    int               linger_timeout;    /* linger timeout */

    int               send_buf_size;     /* send buffer size */
    int               recv_buf_size;     /* recv buffer size */

    struct string     prefix;            /* key prefix */
    req_type_t        method;            /* request type */
    uint32_t          expiry;            /* key expiry */

    struct {
        uint32_t      id;                /* unique client id */
        uint32_t      n;                 /* # client */
    } client;                            /* client */

    uint32_t          num_conns;         /* # connections */
    uint32_t          num_calls;         /* # calls */

    struct dist_opt   conn_dopt;         /* conn distribution option */
    struct dist_opt   call_dopt;         /* call distribution option */
    struct dist_opt   size_dopt;         /* size distribution option */

    unsigned          print_histogram:1; /* print response time histogram? */
    unsigned          disable_nodelay:1; /* disable_nodelay? */
    unsigned          print_rusage:1;    /* print rusage? */
    unsigned          linger:1;          /* linger? */
    unsigned          use_noreply:1;     /* use_noreply? */
};

struct context {
    struct opt         opt;                     /* cmdline option */

    int                ep;                      /* epoll descriptor */
    struct epoll_event *event;                  /* epoll event */
    int                nevent;                  /* # epoll event */
    int                timeout;                 /* epoll timeout */

    uint32_t           nconn_created;           /* # connection created */
    uint32_t           nconn_create_failed;     /* # connection create failed */
    uint32_t           nconn_destroyed;         /* # connection destroyed */

    struct dist_info   conn_dist;               /* conn generator distribution */
    struct dist_info   call_dist;               /* call generator distribution */
    struct dist_info   size_dist;               /* size generator distribution */

    struct gen         conn_gen;                /* connection generator */
    struct gen         size_gen;                /* size generator */

    struct action      action[MAX_EVENT_TYPES]; /* event actions */

    struct stats       stats;                   /* statistics */

    char               buf1m[MB];               /* 1M buffer */
};

typedef void (*init_t)(struct context *, void *);
typedef void (*deinit_t)(struct context *, void *);
typedef void (*dump_t)(struct context *, void *);
typedef void (*start_t)(struct context *, void *);
typedef void (*stop_t)(struct context *, void *);

struct load_generator {
    char      *name;    /* generator name */
    init_t    init;     /* init generator */
    deinit_t  deinit;   /* deinit generator */
    start_t   start;    /* start generator */
    stop_t    stop;     /* stop generator */
};

struct stats_collector {
    char      *name;    /* collector name */
    init_t    init;     /* init collector */
    start_t   start;    /* start collector */
    stop_t    stop;     /* stop collector */
    dump_t    dump;     /* dump collector */
};

rstatus_t core_init(struct context *ctx);
void core_deinit(struct context *ctx);

void core_start(struct context *ctx);
void core_stop(struct context *ctx);
rstatus_t core_loop(struct context *ctx);

rstatus_t core_connect(struct context *ctx, struct conn *conn);
void core_send(struct context *ctx, struct conn *conn);
void core_recv(struct context *ctx, struct conn *conn);
void core_close(struct context *ctx, struct conn *conn);
void core_error(struct context *ctx, struct conn *conn);

void core_timeout(struct timer *t, void *arg);

#endif
