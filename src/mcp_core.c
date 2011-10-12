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

#include <sys/epoll.h>

#include <mcp_core.h>

extern struct load_generator size_generator, conn_generator, call_generator;
extern struct stats_collector conn_stats, call_stats;

static struct load_generator *gen[] = {   /* load generators */
    &size_generator,
    &conn_generator,
    &call_generator
};

static struct stats_collector *col[] = {  /* stats collectors */
    &conn_stats,
    &call_stats
};

rstatus_t
core_init(struct context *ctx)
{
    rstatus_t status;
    struct opt *opt = &ctx->opt;
    uint32_t i;

    /* initialize event machine */
    ctx->timeout = TIMER_INTERVAL * 1e3;
    ctx->nevent = (int)opt->num_conns;
    status = event_init(ctx, EVENT_SIZE_HINT);
    if (status != MCP_OK) {
        return status;
    }

    /* initialize connection subsystem */
    conn_init();

    /* initialize call subsystem */
    call_init();

    /* initialize the stats collectors before the load generators */
    for (i = 0; i < NELEM(col); i++) {
        col[i]->init(ctx, NULL);
    }

    /* initialize the load generators */
    for (i = 0; i < NELEM(gen); i++) {
        gen[i]->init(ctx, NULL);
    }

    return MCP_OK;
}

void
core_deinit(struct context *ctx)
{
}

void
core_start(struct context *ctx)
{
    uint32_t i;

    /* start the stats subsystem */
    stats_start(ctx);

    /* start stats collectors */
    for (i = 0; i < NELEM(col); i++) {
        col[i]->start(ctx, NULL);
    }

    /*
     * Before the connection generator is triggered, all its dependent
     * generators must be triggered.
     */
    ecb_signal(ctx, EVENT_GEN_SIZE_TRIGGER, NULL);

    /* start the connection generator by triggering it */
    ecb_signal(ctx, EVENT_GEN_CONN_TRIGGER, NULL);
}

void
core_stop(struct context *ctx)
{
    mcp_free(ctx->event);
    close(ctx->ep);

    stats_dump(ctx);
}

void
core_timeout(struct timer *t, void *arg)
{
    struct conn *conn = arg;
    struct context *ctx = conn->ctx;

    log_debug(LOG_INFO, "c %"PRIu64" on sd %d timedout", conn->id, conn->sd);

    /* timer are freed by the timeout handler */
    ASSERT(conn->watchdog == t);
    conn->watchdog = NULL;

    conn->connecting = 0;

    ecb_signal(ctx, EVENT_CONN_TIMEOUT, conn);
    ecb_signal(ctx, EVENT_CONN_DESTROYED, conn);
}

static rstatus_t
core_connecting(struct context *ctx, struct conn *conn)
{
    struct opt *opt = &ctx->opt;

    ASSERT(!conn->connecting);
    ASSERT(conn->watchdog == NULL);

    if (opt->timeout > 0.0) {
        conn->watchdog = timer_schedule(core_timeout, conn, opt->timeout);
        if (conn->watchdog == NULL) {
            return MCP_ENOMEM;
        }
    }

    conn->connecting = 1;

    log_debug(LOG_VERB, "connecting on c %"PRIu64" sd %d", conn->id, conn->sd);

    return MCP_OK;
}

static void
core_connected(struct context *ctx, struct conn *conn)
{
    struct opt *opt = &ctx->opt;

    ASSERT(conn->connecting);
    ASSERT(!conn->connected);

    log_debug(LOG_DEBUG, "connected on c %"PRIu64" sd %d", conn->id, conn->sd);

    conn->connecting = 0;
    conn->connected = 1;

    if (opt->timeout > 0.0) {
        ASSERT(conn->watchdog != NULL);
        timer_cancel(conn->watchdog);
    }

    ecb_signal(ctx, EVENT_CONN_CONNECTED, conn);

    /*
     * Before a call generator is triggered, we must have a
     * connected connection.
     */
    ecb_signal(ctx, EVENT_GEN_CALL_TRIGGER, conn);
}

rstatus_t
core_connect(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    struct opt *opt = &ctx->opt;
    struct sockinfo *si = &opt->si;

    ASSERT(conn->sd < 0);

    conn->sd = socket(si->family, SOCK_STREAM, 0);
    if (conn->sd < 0) {
	    log_debug(LOG_ERR, "socket create for c %"PRIu64" failed: %s",
                  conn->id, strerror(errno));
        status = MCP_ERROR;
        goto error;
    }

    status = mcp_set_nonblocking(conn->sd);
    if (status != MCP_OK) {
        log_debug(LOG_ERR, "set nonblock on c %"PRIu64" sd %d failed: %s",
                  conn->id, conn->sd, strerror(errno));
        goto error;
    }

    if (!opt->disable_nodelay) {
        status = mcp_set_tcpnodelay(conn->sd);
        if (status != MCP_OK) {
            log_debug(LOG_ERR, "set tcpnodelay on c %"PRIu64" sd %d failed: %s",
                      conn->id, conn->sd, strerror(errno));
            goto error;
        }
    }

    if (opt->linger) {
        status = mcp_set_linger(conn->sd, opt->linger_timeout);
        if (status != MCP_OK) {
            log_debug(LOG_ERR, "set linger on c %"PRIu64" sd %d failed: %s",
                      conn->id, conn->sd, strerror(errno));
            goto error;
        }
    }

    status = mcp_set_sndbuf(conn->sd, opt->send_buf_size);
    if (status != MCP_OK) {
        log_debug(LOG_ERR, "set sndbuf on c %"PRIu64" sd %d to %d failed: %s",
                  conn->id, conn->sd, opt->send_buf_size);
        goto error;
    }

    status = mcp_set_rcvbuf(conn->sd, opt->recv_buf_size);
    if (status != MCP_OK) {
        log_debug(LOG_ERR, "set rcvbuf on c %"PRIu64" sd %d to %d failed: %s",
                  conn->id, conn->sd, opt->recv_buf_size);
        goto error;
    }

    status = event_add_conn(ctx->ep, conn);
    if (status != MCP_OK) {
        log_debug(LOG_ERR, "event add conn e %d sd %d failed: %s", ctx->ep,
                  conn->sd, strerror(errno));
        goto error;
    }

    ecb_signal(ctx, EVENT_CONN_CONNECTING, conn);

    status = connect(conn->sd, (struct sockaddr *)&si->addr, si->addrlen);
    if (status != MCP_OK) {
        if (errno == EINPROGRESS) {
            status = core_connecting(ctx, conn);
            if (status == MCP_OK) {
                return MCP_OK;
            }
        }
        log_debug(LOG_ERR, "connect on c %"PRIu64" sd %d failed: %s", conn->id,
                  conn->sd, strerror(errno));
        goto error;
    }

    ASSERT(!conn->connecting);
    ASSERT(!conn->connected);
    ASSERT(conn->watchdog == NULL);

    conn->connected = 1;

    log_debug(LOG_INFO, "connected on c %"PRIu64" sd %d", conn->id, conn->sd);

    ecb_signal(ctx, EVENT_CONN_CONNECTED, conn);

    /*
     * Before a call generator is triggered, we must have a
     * connected connection.
     */
    ecb_signal(ctx, EVENT_GEN_CALL_TRIGGER, conn);

    return MCP_OK;

error:
    conn->err = errno;
    return status;
}

void
core_send(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    struct call *call;

    if (conn->connecting) {
        core_connected(ctx, conn);
    }

    conn->send_ready = 1;
    do {
        call = STAILQ_FIRST(&conn->call_sendq);
        if (call == NULL) {
            return;
        }

        status = call_send(ctx, call);
        if (status != MCP_OK) {
            return;
        }

    } while (conn->send_ready);
}

void
core_recv(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    struct call *call;

    ASSERT(!conn->connecting);

    conn->recv_ready = 1;
    do {
        call = STAILQ_FIRST(&conn->call_recvq);
        if (call == NULL) {
            return;
        }

        status = call_recv(ctx, call);
        if (status != MCP_OK) {
            return;
        }
    } while (conn->recv_ready);
}

void
core_close(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    struct call *call, *ncall; /* current and next call */

    if (conn->sd < 0) {
        return;
    }

    for (call = STAILQ_FIRST(&conn->call_recvq); call != NULL; call = ncall) {
        ncall = STAILQ_NEXT(call, call_tqe);

        ASSERT(conn->ncall_recvq != 0);

        conn->ncall_recvq--;
        STAILQ_REMOVE(&conn->call_recvq, call, call, call_tqe);
        call_put(call);
    }
    ASSERT(conn->ncall_recvq == 0);

    for (call = STAILQ_FIRST(&conn->call_sendq); call != NULL; call = ncall) {
        ncall = STAILQ_NEXT(call, call_tqe);

        ASSERT(conn->ncall_sendq != 0);

        conn->ncall_sendq--;
        STAILQ_REMOVE(&conn->call_sendq, call, call, call_tqe);
        call_put(call);
    }
    ASSERT(conn->ncall_sendq == 0);

    status = close(conn->sd);
    if (status != MCP_OK) {
        log_debug(LOG_ERR, "close c %"PRIu64" sd %d failed: %s", conn->id,
                  conn->sd, strerror(errno));
    }
    conn->sd = -1;

    conn_put(conn);
}

void
core_error(struct context *ctx, struct conn *conn)
{
    rstatus_t status;

    if (conn->err == 0) {
        status = mcp_get_soerror(conn->sd);
        if (status < 0) {
            log_debug(LOG_ERR, "get soerr on c %"PRIu64" sd %d failed: %s",
                      conn->id, conn->sd, strerror(errno));
        }
        conn->err = errno;
    }

    log_debug(LOG_ERR, "error on c %"PRIu64" sd %d: %s", conn->id, conn->sd,
              strerror(conn->err));

    ecb_signal(ctx, EVENT_CONN_FAILED, conn);
    ecb_signal(ctx, EVENT_CONN_DESTROYED, conn);
}

static void
core_core(struct context *ctx, struct conn *conn, uint32_t events)
{
    if (events & EPOLLERR) {
        core_error(ctx, conn);
        return;
    }

    /* read takes precedence over write */
    if (events & (EPOLLIN | EPOLLHUP)) {
        core_recv(ctx, conn);
        if (conn->eof || conn->err != 0) {
            core_error(ctx, conn);
            return;
        }
    }

    if (events & EPOLLOUT) {
        core_send(ctx, conn);
        if (conn->err != 0) {
            core_error(ctx, conn);
            return;
        }
    }
}

rstatus_t
core_loop(struct context *ctx)
{
    int i, nsd;

    timer_tick();

    nsd = event_wait(ctx->ep, ctx->event, ctx->nevent, ctx->timeout);
    if (nsd < 0) {
        return nsd;
    }

    for (i = 0; i < nsd; i++) {
        struct epoll_event *ev = &ctx->event[i];

        core_core(ctx, ev->data.ptr, ev->events);

        timer_tick();
    }

    return MCP_OK;
}
