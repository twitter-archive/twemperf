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

#ifndef _MCP_CALL_H_
#define _MCP_CALL_H_

#include <sys/uio.h>

#define LF                  (uint8_t) 10
#define CR                  (uint8_t) 13
#define CRLF                "\x0d\x0a"
#define CRLF_LEN            sizeof(CRLF) - 1

#define REQ_CODEC(ACTION)                   \
    ACTION( GET,            "get "         )\
    ACTION( GETS,           "gets "        )\
    ACTION( DELETE,         "delete "      )\
    ACTION( CAS,            "cas "         )\
    ACTION( SET,            "set "         )\
    ACTION( ADD,            "add "         )\
    ACTION( REPLACE,        "replace "     )\
    ACTION( APPEND,         "append "      )\
    ACTION( PREPEND,        "prepend "     )\
    ACTION( INCR,           "incr "        )\
    ACTION( DECR,           "decr "        )\
    ACTION( XXX,            "xxx "         )\

#define RSP_CODEC(ACTION)                   \
    ACTION( STORED,         "STORED"       )\
    ACTION( NOT_STORED,     "NOT_STORED"   )\
    ACTION( EXISTS,         "EXISTS"       )\
    ACTION( NOT_FOUND,      "NOT_FOUND"    )\
    ACTION( END,            "END"          )\
    ACTION( VALUE,          "VALUE"        )\
    ACTION( DELETED,        "DELETED"      )\
    ACTION( ERROR,          "ERROR"        )\
    ACTION( CLIENT_ERROR,   "CLIENT_ERROR" )\
    ACTION( SERVER_ERROR,   "SERVER_ERROR" )\
    ACTION( NUM,            ""             )\

#define MSG_CODEC(ACTION)                   \
    ACTION( NOREPLY,        "noreply"      )\
    ACTION( CRLF,           "\r\n"         )\
    ACTION( ZERO,           "0 "           )\

#define DEFINE_ACTION(_type, _name) REQ_##_type,
typedef enum req_type {
    REQ_CODEC( DEFINE_ACTION )
    REQ_MAX_TYPES
} req_type_t;
#undef DEFINE_ACTION

#define DEFINE_ACTION(_type, _name) RSP_##_type,
typedef enum rsp_type {
    RSP_CODEC( DEFINE_ACTION )
    RSP_MAX_TYPES
} rsp_type_t;
#undef DEFINE_ACTION

#define DEFINE_ACTION(_type, _name) MSG_##_type,
typedef enum msg_type {
    REQ_CODEC( DEFINE_ACTION )
    RSP_CODEC( DEFINE_ACTION )
    MSG_CODEC( DEFINE_ACTION )
    MSG_MAX_TYPES
} msg_type_t;
#undef DEFINE_ACTION

typedef enum req_iov {
    REQ_IOV_METHOD,
    REQ_IOV_KEY,
    REQ_IOV_FLAG,
    REQ_IOV_EXPIRY,
    REQ_IOV_VLEN,
    REQ_IOV_CAS,
    REQ_IOV_NOREPLY,
    REQ_IOV_CRLF,
    REQ_IOV_VALUE,
    REQ_IOV_CRLF2,
    REQ_IOV_LEN
} req_iov_t;

#define UINT32_MAX_LEN  10

#define CALL_PREFIX_LEN     16
#define CALL_ID_LEN         8
#define CALL_KEYNAME_LEN    (CALL_PREFIX_LEN + CALL_ID_LEN)
#define CALL_EXPIRY_LEN     UINT32_MAX_LEN
#define CALL_KEYLEN_LEN     UINT32_MAX_LEN

/*
 * A call is the basic unit representing a single request followed by
 * a response. A call is tied to a single connection and a given
 * connection can have multiple outstanding calls on it.
 */
struct call {
    STAILQ_ENTRY(call) call_tqe;                   /* link in send / recv / free q */
    uint64_t           id;                         /* unique id */
    struct conn        *conn;                      /* owner connection */

    struct {
        char            keyname[CALL_KEYNAME_LEN]; /* key name */
        char            expiry[CALL_EXPIRY_LEN];   /* expiry in ascii */
        char            keylen[CALL_KEYLEN_LEN];   /* key length in ascii */
        size_t          send;                      /* bytes to send */
        size_t          sent;                      /* bytes sent */
        double          issue_start;               /* issue start time in sec */
        double          send_start;                /* send start time in sec */
        double          send_stop;                 /* send stop time in sec */
        struct iovec    iov[REQ_IOV_LEN];          /* request iov */
        unsigned        noreply:1;                 /* noreply? */
        unsigned        sending:1;                 /* sending call? */
    } req;                                         /* request */

    struct {
        double           recv_start;               /* recv start time in sec */
        size_t           rcvd;                     /* bytes received */
        char             *rcurr;                   /* recv marker */
        size_t           rsize;                    /* recv buffer size */
        char             *pcurr;                   /* parsing marker */
        char             *start;                   /* start marker */
        char             *end;                     /* end marker */
        rsp_type_t       type;                     /* parsed response type? */
        uint32_t         vlen;                     /* value length + crlf length */
        unsigned         parsed_line:1;            /* parsed line? */
        unsigned         parsed_vlen:1;            /* parsed vlen? */
    } rsp;                                         /* response */
};

STAILQ_HEAD(call_tqh, call);

struct call *call_get(struct conn *conn);
void call_put(struct call *call);

ssize_t call_sendv(struct call *call, struct iovec *iov, int iovcnt);
void call_make_req(struct context *ctx, struct call *call);

rstatus_t call_send(struct context *ctx, struct call *call);
rstatus_t call_recv(struct context *ctx, struct call *call);

void call_init(void);
void call_deinit(void);

#endif
