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

#include <sys/uio.h>

#include <mcp_core.h>

static int nfree_connq;            /* # free conn q */
static struct conn_tqh free_connq; /* free conn q */
static uint64_t id;

struct conn *
conn_get(struct context *ctx)
{
    struct conn *conn;

    if (!STAILQ_EMPTY(&free_connq)) {
        ASSERT(nfree_connq > 0);

        conn = STAILQ_FIRST(&free_connq);
        nfree_connq--;

        STAILQ_REMOVE_HEAD(&free_connq, conn_tqe);
    } else {
        conn = mcp_alloc(sizeof(*conn));
        if (conn == NULL) {
            return NULL;
        }
    }

    STAILQ_NEXT(conn, conn_tqe) = NULL;
    conn->id = ++id;
    conn->ctx = ctx;

    conn->ncall_sendq = 0;
    STAILQ_INIT(&conn->call_sendq);
    conn->ncall_recvq = 0;
    STAILQ_INIT(&conn->call_recvq);

    conn->watchdog = NULL;
    conn->connect_start = 0.0;

    conn->sd = -1;

    /* conn->call_gen is initialized later */
    conn->ncall_created = 0;
    conn->ncall_create_failed = 0;
    conn->ncall_completed = 0;

    conn->err = 0;
    conn->recv_active = 0;
    conn->recv_ready = 0;
    conn->send_active = 0;
    conn->send_ready = 0;

    conn->connecting = 0;
    conn->connected = 0;
    conn->eof = 0;

    log_debug(LOG_VVERB, "get conn %p id %"PRIu64"", conn, conn->id);

    return conn;
}

void
conn_put(struct conn *conn)
{
    log_debug(LOG_VVERB, "put conn %p id %"PRIu64"", conn, conn->id);

    nfree_connq++;
    STAILQ_INSERT_TAIL(&free_connq, conn, conn_tqe);
}

static void
conn_free(struct conn *conn)
{
    log_debug(LOG_VVERB, "free conn %p id %"PRIu64"", conn, conn->id);
    mcp_free(conn);
}

ssize_t
conn_sendv(struct conn *conn, struct iovec *iov, int iovcnt, size_t iov_size)
{
    ssize_t n;

    ASSERT(iov_size != 0);
    ASSERT(conn->send_ready);

    for (;;) {
        n = writev(conn->sd, iov, iovcnt);

        log_debug(LOG_VERB, "sendv on c %"PRIu64" sd %d %zd of %zu in "
                  "%"PRIu32" buffers", conn->id, conn->sd, n, iov_size,
                  iovcnt);

        if (n > 0) {
            if (n < (ssize_t) iov_size) {
                conn->send_ready = 0;
            }
            return n;
        }

        if (n == 0) {
            log_warn("sendv on c %"PRIu64" sd %d returned zero", conn->id,
                     conn->sd);
            conn->send_ready = 0;
            return 0;
        }

        if (errno == EINTR) {
            log_debug(LOG_VERB, "sendv on c %"PRIu64" sd %d not ready - eintr",
                      conn->id, conn->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            conn->send_ready = 0;
            log_debug(LOG_VERB, "sendv on c %"PRIu64" sd %d not ready - "
                      "eagain", conn->id, conn->sd);
            return MCP_EAGAIN;
        } else {
            conn->send_ready = 0;
            conn->err = errno;
            log_debug(LOG_ERR, "sendv on c %"PRIu64" sd %d failed: %s",
                      conn->id, conn->sd, strerror(errno));
            return MCP_EAGAIN;
        }
    }

    NOT_REACHED();
}

ssize_t
conn_recv(struct conn *conn, void *buf, size_t size)
{
    ssize_t n;

    ASSERT(buf != NULL);
    ASSERT(size > 0);
    ASSERT(conn->recv_ready);

    for (;;) {
        n = read(conn->sd, buf, size);

        log_debug(LOG_VERB, "recv on sd %d %zd of %zu", conn->sd, n, size);

        if (n > 0) {
            if (n < (ssize_t) size) {
                conn->recv_ready = 0;
            }
            return n;
        }

        if (n == 0) {
            conn->recv_ready = 0;
            conn->eof = 1;
            log_debug(LOG_INFO, "recv on sd %d eof", conn->sd);
            return n;
        }

        if (errno == EINTR) {
            log_debug(LOG_VERB, "recv on sd %d not ready - eintr", conn->sd);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            conn->recv_ready = 0;
            log_debug(LOG_VERB, "recv on sd %d not ready - eagain", conn->sd);
            return MCP_EAGAIN;
        } else {
            conn->recv_ready = 0;
            conn->err = errno;
            log_error("recv on sd %d failed: %s", conn->sd, strerror(errno));
            return MCP_ERROR;
        }
    }

    NOT_REACHED();
}

void
conn_init(void)
{
    nfree_connq = 0;
    STAILQ_INIT(&free_connq);
}

void
conn_deinit(void)
{
    struct conn *conn, *nconn; /* current and next connection */

    for (conn = STAILQ_FIRST(&free_connq); conn != NULL;
         conn = nconn, nfree_connq--) {
        ASSERT(nfree_connq > 0);
        nconn = STAILQ_NEXT(conn, conn_tqe);
        conn_free(conn);
    }
    ASSERT(nfree_connq == 0);
}
