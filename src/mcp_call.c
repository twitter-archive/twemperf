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

#include <mcp_core.h>

static int nfree_callq;            /* # free call q */
static struct call_tqh free_callq; /* free call q */
static uint64_t id;                /* call id counter */

#define DEFINE_ACTION(_type, _name) { _name, sizeof(_name) - 1 },
struct string req_strings[] = {
    REQ_CODEC( DEFINE_ACTION )
    { NULL, 0 }
};
#undef DEFINE_ACTION

#define DEFINE_ACTION(_type, _name) { _name, sizeof(_name) - 1 },
struct string rsp_strings[] = {
    RSP_CODEC( DEFINE_ACTION )
    { NULL, 0 }
};
#undef DEFINE_ACTION

#define DEFINE_ACTION(_type, _name) { _name, sizeof(_name) - 1 },
struct string msg_strings[] = {
    REQ_CODEC( DEFINE_ACTION )
    RSP_CODEC( DEFINE_ACTION )
    MSG_CODEC( DEFINE_ACTION )
    { NULL, 0 }
};
#undef DEFINE_ACTION

struct call *
call_get(struct conn *conn)
{
    struct call *call;
    uint32_t i;

    if (!STAILQ_EMPTY(&free_callq)) {
        ASSERT(nfree_callq > 0);

        call = STAILQ_FIRST(&free_callq);
        nfree_callq--;

        STAILQ_REMOVE_HEAD(&free_callq, call_tqe);
    } else {
        call = mcp_alloc(sizeof(*call));
        if (call == NULL) {
            return NULL;
        }
    }

    STAILQ_NEXT(call, call_tqe) = NULL;
    call->id = ++id;
    call->conn = conn;

    /* keyname, expiry and keylen are initialized later */
    call->req.send = 0;
    call->req.sent = 0;
    call->req.issue_start = 0.0;
    call->req.send_start = 0.0;
    call->req.send_stop = 0.0;
    for (i = 0; i < REQ_IOV_LEN; i++) {
        call->req.iov[i].iov_base = NULL;
        call->req.iov[i].iov_len = 0;
    }
    call->req.noreply = 0;
    call->req.sending = 0;

    call->rsp.recv_start = 0.0;
    call->rsp.rcvd = 0;
    call->rsp.rcurr = conn->buf;
    call->rsp.rsize = sizeof(conn->buf);
    call->rsp.pcurr = call->rsp.rcurr;
    call->rsp.start = NULL;
    call->rsp.end = NULL;
    call->rsp.type = 0;
    call->rsp.vlen = 0;
    call->rsp.parsed_line = 0;
    call->rsp.parsed_vlen = 0;

    log_debug(LOG_VVERB, "get call %p id %"PRIu64"", call, call->id);

    return call;
}

void
call_put(struct call *call)
{
    log_debug(LOG_VVERB, "put call %p id %"PRIu64"", call, call->id);

    nfree_callq++;
    STAILQ_INSERT_TAIL(&free_callq, call, call_tqe);
}

static void
call_free(struct call *call)
{
    log_debug(LOG_VVERB, "free call %p id %"PRIu64"", call, call->id);
    mcp_free(call);
}

void
call_init(void)
{
    nfree_callq = 0;
    STAILQ_INIT(&free_callq);
}

void
call_deinit(void)
{
    struct call *call, *ncall; /* current and next call */

    for (call = STAILQ_FIRST(&free_callq); call != NULL;
         call = ncall, nfree_callq--) {
        ASSERT(nfree_callq > 0);
        ncall = STAILQ_NEXT(call, call_tqe);
        call_free(call);
    }
    ASSERT(nfree_callq == 0);
}

static rstatus_t
call_start_timer(struct context *ctx, struct call *call)
{
    struct opt *opt = &ctx->opt;
    struct conn *conn = call->conn;
    double timeout;

    ASSERT(!STAILQ_EMPTY(&conn->call_recvq));

    if (opt->timeout == 0.0) {
        return MCP_OK;
    }

    if (call != STAILQ_FIRST(&conn->call_recvq)) {
        /*
         * Watcdog timer has already been scheduled by a previous call
         * which is still outstanding on this connection.
         */
        ASSERT(conn->watchdog != NULL);
        return MCP_OK;
    }

    ASSERT(conn->watchdog == NULL);
    ASSERT(call->req.send_stop > 0.0);
    ASSERT(timer_now() >= call->req.send_stop);
    ASSERT(opt->timeout > (timer_now() - call->req.send_stop));

    timeout = opt->timeout;
    timeout -= timer_now() - call->req.send_stop;
    conn->watchdog = timer_schedule(core_timeout, conn, timeout);
    if (conn->watchdog == NULL) {
        return MCP_ENOMEM;
    }

    return MCP_OK;
}

static rstatus_t
call_reset_timer(struct context *ctx, struct call *call)
{
    struct opt *opt = &ctx->opt;
    struct conn *conn = call->conn;

    if (opt->timeout == 0.0) {
        return MCP_OK;
    }

    ASSERT(conn->watchdog != NULL);
    timer_cancel(conn->watchdog);

    if (STAILQ_EMPTY(&conn->call_recvq)) {
        /*
         * Skip scheduling a timer as there are no outstanding calls
         * on this connection
         */
        return MCP_OK;
    }

    return call_start_timer(ctx, STAILQ_FIRST(&conn->call_recvq));
}

static void
call_make_retrieval_req(struct context *ctx, struct call *call,
                        uint32_t key_id)
{
    struct opt *opt = &ctx->opt;
    int len;
    uint32_t i;

    /* retrieval request are never a noreply */
    call->req.noreply = 0;

    for (i = 0; i < REQ_IOV_LEN; i++) {
        struct iovec *iov = &call->req.iov[i];

        switch (i) {
        case REQ_IOV_METHOD:
            iov->iov_base = req_strings[opt->method].data;
            iov->iov_len = req_strings[opt->method].len;
            break;

        case REQ_IOV_KEY:
            len = mcp_scnprintf(call->req.keyname, sizeof(call->req.keyname),
                                "%.*s%08"PRIx32" ", opt->prefix.len,
                                opt->prefix.data, key_id);
            iov->iov_base = call->req.keyname;
            iov->iov_len = (size_t)len;
            break;

        case REQ_IOV_CRLF:
            iov->iov_base = msg_strings[MSG_CRLF].data;
            iov->iov_len = msg_strings[MSG_CRLF].len;
            break;

        case REQ_IOV_FLAG:
        case REQ_IOV_EXPIRY:
        case REQ_IOV_VLEN:
        case REQ_IOV_CAS:
        case REQ_IOV_NOREPLY:
        case REQ_IOV_VALUE:
        case REQ_IOV_CRLF2:
            iov->iov_base = NULL;
            iov->iov_len = 0;
            break;

        default:
            NOT_REACHED();
        }
        call->req.send += iov->iov_len;
    }
}

static void
call_make_delete_req(struct context *ctx, struct call *call, uint32_t key_id)
{
    struct opt *opt = &ctx->opt;
    int len;
    uint32_t i;

    for (i = 0; i < REQ_IOV_LEN; i++) {
        struct iovec *iov = &call->req.iov[i];

        switch (i) {
        case REQ_IOV_METHOD:
            iov->iov_base = req_strings[opt->method].data;
            iov->iov_len = req_strings[opt->method].len;
            break;

        case REQ_IOV_KEY:
            len = mcp_scnprintf(call->req.keyname, sizeof(call->req.keyname),
                                "%.*s%08"PRIx32" ", opt->prefix.len,
                                opt->prefix.data, key_id);
            iov->iov_base = call->req.keyname;
            iov->iov_len = (size_t)len;
            break;

        case REQ_IOV_FLAG:
        case REQ_IOV_EXPIRY:
        case REQ_IOV_VLEN:
        case REQ_IOV_CAS:
            iov->iov_base = NULL;
            iov->iov_len = 0;
            break;

        case REQ_IOV_NOREPLY:
            if (opt->use_noreply) {
                iov->iov_base = msg_strings[MSG_NOREPLY].data;
                iov->iov_len = msg_strings[MSG_NOREPLY].len;
                call->req.noreply = 1;
            } else {
                iov->iov_base = NULL;
                iov->iov_len = 0;
                call->req.noreply = 0;
            }
            break;

        case REQ_IOV_CRLF:
            iov->iov_base = msg_strings[MSG_CRLF].data;
            iov->iov_len = msg_strings[MSG_CRLF].len;
            break;

        case REQ_IOV_VALUE:
        case REQ_IOV_CRLF2:
            iov->iov_base = NULL;
            iov->iov_len = 0;
            break;

        default:
            NOT_REACHED();
        }
        call->req.send += iov->iov_len;
    }
}

static void
call_make_storage_req(struct context *ctx, struct call *call, uint32_t key_id,
                      long int key_vlen)
{
    struct opt *opt = &ctx->opt;
    int len;
    uint32_t i;

    for (i = 0; i < REQ_IOV_LEN; i++) {
        struct iovec *iov = &call->req.iov[i];

        switch (i) {
        case REQ_IOV_METHOD:
            iov->iov_base = req_strings[opt->method].data;
            iov->iov_len = req_strings[opt->method].len;
            break;

        case REQ_IOV_KEY:
            len = mcp_scnprintf(call->req.keyname, sizeof(call->req.keyname),
                                "%.*s%08"PRIx32" ", opt->prefix.len,
                                opt->prefix.data, key_id);
            iov->iov_base = call->req.keyname;
            iov->iov_len = (size_t)len;
            break;

        case REQ_IOV_FLAG:
            iov->iov_base = msg_strings[MSG_ZERO].data;
            iov->iov_len  = msg_strings[MSG_ZERO].len;
            break;

        case REQ_IOV_EXPIRY:
            len = mcp_scnprintf(call->req.expiry, sizeof(call->req.expiry),
                                "%"PRIu32" ", opt->expiry);
            iov->iov_base = call->req.expiry;
            iov->iov_len = (size_t)len;
            break;

        case REQ_IOV_VLEN:
            len = mcp_scnprintf(call->req.keylen, sizeof(call->req.keylen),
                                "%ld ", key_vlen);
            iov->iov_base = call->req.keylen;
            iov->iov_len = (size_t)len;
            break;

        case REQ_IOV_CAS:
            if (opt->method == REQ_CAS) {
                iov->iov_base = "1 ";
                iov->iov_len = 2;
            } else {
                iov->iov_base = NULL;
                iov->iov_len = 0;
            }
            break;

        case REQ_IOV_NOREPLY:
            if (opt->use_noreply) {
                iov->iov_base = msg_strings[MSG_NOREPLY].data;
                iov->iov_len = msg_strings[MSG_NOREPLY].len;
                call->req.noreply = 1;
            } else {
                iov->iov_base = NULL;
                iov->iov_len = 0;
                call->req.noreply = 0;
            }
            break;

        case REQ_IOV_CRLF:
            iov->iov_base = msg_strings[MSG_CRLF].data;
            iov->iov_len = msg_strings[MSG_CRLF].len;
            break;

        case REQ_IOV_VALUE:
            ASSERT(key_vlen >= 0 && key_vlen <= sizeof(ctx->buf1m));
            iov->iov_base = ctx->buf1m;
            iov->iov_len = (size_t)key_vlen;
            break;

        case REQ_IOV_CRLF2:
            iov->iov_base = msg_strings[MSG_CRLF].data;
            iov->iov_len = msg_strings[MSG_CRLF].len;
            break;

        default:
            NOT_REACHED();
        }
        call->req.send += iov->iov_len;
    }
}

static void
call_make_arithmetic_req(struct context *ctx, struct call *call,
                         uint32_t key_id, long int key_vlen)
{
    struct opt *opt = &ctx->opt;
    int len;
    uint32_t i;

    for (i = 0; i < REQ_IOV_LEN; i++) {
        struct iovec *iov = &call->req.iov[i];

        switch (i) {
        case REQ_IOV_METHOD:
            iov->iov_base = req_strings[opt->method].data;
            iov->iov_len = req_strings[opt->method].len;
            break;

        case REQ_IOV_KEY:
            len = mcp_scnprintf(call->req.keyname, sizeof(call->req.keyname),
                                "%.*s%08"PRIx32" ", opt->prefix.len,
                                opt->prefix.data, key_id);
            iov->iov_base = call->req.keyname;
            iov->iov_len = (size_t)len;
            break;

        case REQ_IOV_FLAG:
            iov->iov_base = NULL;
            iov->iov_len = 0;
            break;

        case REQ_IOV_EXPIRY:
            /* use expiry iov as incr/decr value */
            len = mcp_scnprintf(call->req.expiry, sizeof(call->req.expiry),
                                "%ld ", key_vlen);
            iov->iov_base = call->req.expiry;
            iov->iov_len = (size_t)len;
            break;

        case REQ_IOV_VLEN:
        case REQ_IOV_CAS:
            iov->iov_base = NULL;
            iov->iov_len = 0;
            break;

        case REQ_IOV_NOREPLY:
            if (opt->use_noreply) {
                iov->iov_base = msg_strings[MSG_NOREPLY].data;
                iov->iov_len = msg_strings[MSG_NOREPLY].len;
                call->req.noreply = 1;
            } else {
                iov->iov_base = NULL;
                iov->iov_len = 0;
                call->req.noreply = 0;
            }
            break;

        case REQ_IOV_CRLF:
            iov->iov_base = msg_strings[MSG_CRLF].data;
            iov->iov_len = msg_strings[MSG_CRLF].len;
            break;

        case REQ_IOV_VALUE:
        case REQ_IOV_CRLF2:
            iov->iov_base = NULL;
            iov->iov_len = 0;
            break;

        default:
            NOT_REACHED();
        }
        call->req.send += iov->iov_len;
    }
}

void
call_make_req(struct context *ctx, struct call *call)
{
    struct opt *opt = &ctx->opt;
    struct dist_info *di = &ctx->size_dist;
    uint32_t key_id;
    long int key_vlen;

    call->req.send = 0;
    call->req.sent = 0;

    /*
     * Get the current item id and size from the distribution, and
     * call into the size generator to move to the next value
     */
    key_id = di->next_id;
    key_vlen = lrint(di->next_val);
    ecb_signal(ctx, EVENT_GEN_SIZE_FIRE, &ctx->size_gen);

    switch (opt->method) {
    case REQ_GET:
    case REQ_GETS:
        call_make_retrieval_req(ctx, call, key_id);
        break;

    case REQ_DELETE:
        call_make_delete_req(ctx, call, key_id);
        break;

    case REQ_CAS:
    case REQ_SET:
    case REQ_ADD:
    case REQ_REPLACE:
    case REQ_APPEND:
    case REQ_PREPEND:
    case REQ_XXX:
        call_make_storage_req(ctx, call, key_id, key_vlen);
        break;

    case REQ_INCR:
    case REQ_DECR:
        call_make_arithmetic_req(ctx, call, key_id, key_vlen);
        break;

    default:
        NOT_REACHED();
    }
}

rstatus_t
call_send(struct context *ctx, struct call *call)
{
    struct conn *conn = call->conn;
    size_t sent;
    ssize_t n;
    uint32_t i;

    ASSERT(call->req.send != 0);

    if (!call->req.sending) {
        /*
         * We might need multiple write events to send a call completely.
         * Signal the first time we start sending a given call.
         */
        ecb_signal(ctx, EVENT_CALL_SEND_START, call);
        call->req.sending = 1;
    }

    n = conn_sendv(conn, call->req.iov, REQ_IOV_LEN, call->req.send);

    sent = n > 0 ? (size_t)n : 0;

    log_debug(LOG_VERB, "send call %"PRIu64" on c %"PRIu64" sd %d %zu of %zu "
              "bytes", call->id, conn->id, conn->sd, sent, call->req.send);

    call->req.send -= sent;
    call->req.sent += sent;

    for (i = 0; i < REQ_IOV_LEN; i++) {
        struct iovec *iov = &call->req.iov[i];

        if (sent == 0) {
            break;
        }

        if (sent < iov->iov_len) {
            /* iov element was sent partially; send remaining bytes later */
            iov->iov_base = (char *)iov->iov_base + sent;
            iov->iov_len -= sent;
            sent = 0;
            break;
        }

        /* iov element was sent completely; mark it empty */
        sent -= iov->iov_len;
        iov->iov_base = NULL;
        iov->iov_len = 0;
    }

    if (call->req.send == 0) {
        ecb_signal(ctx, EVENT_CALL_SEND_STOP, call);

        /*
         * Call has been sent completely; move the call from sendq
         * to recvq unless it has been marked as noreply.
         */
        conn->ncall_sendq--;
        STAILQ_REMOVE(&conn->call_sendq, call, call, call_tqe);

        if (call->req.noreply) {
            ecb_signal(ctx, EVENT_CALL_DESTROYED, call);
            call_put(call);
        } else {
            STAILQ_INSERT_TAIL(&conn->call_recvq, call, call_tqe);
            conn->ncall_recvq++;
            call_start_timer(ctx, call);
        }
    }

    if (n > 0) {
        return MCP_OK;
    }

    return (n == MCP_EAGAIN) ? MCP_OK : MCP_ERROR;
}

static rstatus_t
call_parse_rsp_line(struct context *ctx, struct call *call)
{
    char *p, *q;
    int size;
    struct string *str;

    if (call->rsp.parsed_line) {
        return MCP_OK;
    }

    ASSERT(call->rsp.rcurr > call->rsp.pcurr);

    size = call->rsp.rcurr - call->rsp.pcurr;

    p = call->rsp.pcurr;
    q = mcp_memchr(p, '\n', size);
    if (q == NULL) {
        return MCP_EAGAIN;
    }
    ASSERT(*(q - 1) == '\r');
    q = q + 1;

    /* update the start and end marker of response line */
    call->rsp.start = p;
    call->rsp.end = q;

    /* update the parsing marker */
    call->rsp.pcurr = q;

    for (str = &rsp_strings[0]; str->data != NULL; str++) {
        if ((str->len < (call->rsp.end - call->rsp.start)) &&
            strncmp(p, str->data, str->len) == 0) {
            call->rsp.type = str - rsp_strings;
            call->rsp.parsed_line = 1;
            return MCP_OK;
        }
    }

    return MCP_ERROR;
}

static rstatus_t
call_parse_rsp_vlen(struct context *ctx, struct call *call)
{
    char *p, *q;
    int token;

    p = call->rsp.start;
    q = call->rsp.end;

    /*
     * Parse value line with format:
     *   VALUE <key> <flags> <datalen>\r\n<data>\r\n
     */
    token = 0;
    while (p < q) {
        if (*p != ' ') {
            p++;
            continue;
        }

        token++;

        /* skip multiple spaces */
        while (*p == ' ') {
            p++;
        }

        if (token == 3) {
            break;
        }
    }

    if (token != 3) {
        return MCP_EAGAIN;
    }

    call->rsp.vlen = 0;
    while (p < q) {
        if (*p < '0' || *p > '9') {
            break;
        }

        call->rsp.vlen = call->rsp.vlen * 10 + (uint32_t)(*p - '0');
        p++;
    }

    p = (p != q) ? p : p - 1;
    q = mcp_memchr(p, '\n', q - p);
    if (q == NULL) {
        call->rsp.vlen = 0;
        return MCP_EAGAIN;
    }

    call->rsp.vlen += (sizeof("\r\n") - 1) + (sizeof("END\r\n") - 1);
    call->rsp.parsed_vlen = 1;

    return MCP_OK;
}

static rstatus_t
call_parse_rsp_value(struct context *ctx, struct call *call)
{
    rstatus_t status;
    struct conn *conn = call->conn;
    size_t size;

    if (!call->rsp.parsed_vlen) {
        status = call_parse_rsp_vlen(ctx, call);
        if (status != MCP_OK) {
            return status;
        }
        ASSERT(call->rsp.parsed_vlen);
    }

    ASSERT(call->rsp.rcurr >= call->rsp.pcurr);

    size = (size_t)(call->rsp.rcurr - call->rsp.pcurr);

    if (call->rsp.vlen < size) {
        /*
         * Unparsed data in the read buffer after vlen bytes
         * should be parsed as a response for the next calls
         */
        call->rsp.pcurr = call->rsp.pcurr + call->rsp.vlen;
        call->rsp.vlen = 0;
        return MCP_OK;
    }

    call->rsp.vlen -= size;

    /*
     * We have parsed all the data in the read buffer. Reset the read
     * marker to make more space in the read buffer
     */
    call->rsp.rcurr = conn->buf;
    call->rsp.rsize = sizeof(conn->buf);
    call->rsp.pcurr = call->rsp.rcurr;

    return call->rsp.vlen == 0 ? MCP_OK : MCP_EAGAIN;
}

static rstatus_t
call_parse_rsp(struct context *ctx, struct call *call)
{
    rstatus_t status;

    /*
     * Parse the response line until crlf is encountered to
     * determine the response type.
     */
    status = call_parse_rsp_line(ctx, call);
    if (status != MCP_OK) {
        return status;
    }

    /*
     * After we have parsed the response line and determined that it is a
     * value response, we need to parse remaining data until we encounter
     * data of size value length followed by crlf.
     */
    if (call->rsp.type == RSP_VALUE) {
       return call_parse_rsp_value(ctx, call);
    }

    return MCP_OK;
}

rstatus_t
call_recv(struct context *ctx, struct call *call)
{
    rstatus_t status;
    struct conn *conn = call->conn;
    ssize_t n;
    size_t rcvd;

    if (call->rsp.rsize == 0) {
        size_t chunk_size;

        ASSERT(call->rsp.rcurr > call->rsp.pcurr);

        /*
         * Make space in the read buffer by moving the unparsed chunk
         * at the tail end to the head.
         */
        chunk_size = (size_t)(call->rsp.rcurr - call->rsp.pcurr);
        mcp_memmove(conn->buf, call->rsp.pcurr, chunk_size);
        call->rsp.pcurr = conn->buf;
        call->rsp.rcurr = conn->buf + chunk_size;
        call->rsp.rsize = sizeof(conn->buf) - chunk_size;
    }

    if (call->rsp.rcvd == 0) {
        ecb_signal(ctx, EVENT_CALL_RECV_START, call);
    }

    n = conn_recv(conn, call->rsp.rcurr, call->rsp.rsize);

    rcvd = n > 0 ? (size_t)n : 0;

    call->rsp.rcvd += rcvd;
    call->rsp.rcurr += rcvd;
    call->rsp.rsize -= rcvd;

    if (n <= 0) {
        if (n == 0 || n == MCP_EAGAIN) {
            return MCP_OK;
        }
        return MCP_ERROR;
    }

    do {
        struct call *next_call; /* next call in recv q */

        status = call_parse_rsp(ctx, call);
        if (status != MCP_OK) {
            if (status == MCP_EAGAIN) {
                /* incomplete response; parse again when more data arrives */
                return MCP_OK;
            }
            return status;
        }

        next_call = NULL;
        /*
         * Spill over unparsed response onto the next call and update
         * the current call appropriately
         */
        if (call->rsp.rcurr != call->rsp.pcurr) {
            next_call = STAILQ_NEXT(call, call_tqe);
            if (next_call == NULL) {
                log_debug(LOG_ERR, "stray response type %d on c %"PRIu64"",
                          call->rsp.type, conn->id);
                conn->err = EINVAL;
                return MCP_ERROR;
            }

            ecb_signal(ctx, EVENT_CALL_RECV_START, next_call);

            next_call->rsp.rcurr = call->rsp.rcurr;
            next_call->rsp.rsize = call->rsp.rsize;
            next_call->rsp.pcurr = call->rsp.pcurr;

            /*
             * Calculate the exact bytes received on this call and accumulate
             * the remaining bytes onto the next call
             */
            ASSERT(call->rsp.rcurr > call->rsp.pcurr);
            next_call->rsp.rcvd = (size_t)(call->rsp.rcurr - call->rsp.pcurr);
            call->rsp.rcvd -= next_call->rsp.rcvd;
        }

        conn->ncall_recvq--;
        STAILQ_REMOVE(&conn->call_recvq, call, call, call_tqe);

        call_reset_timer(ctx, call);

        ecb_signal(ctx, EVENT_CALL_RECV_STOP, call);
        ecb_signal(ctx, EVENT_CALL_DESTROYED, call);

        call_put(call);

        call = next_call;
    } while (call != NULL);

    return MCP_OK;
}
