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

static void
conn_created(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;

    ASSERT(type == EVENT_CONN_CREATED);

    stats->nconn_created++;
}

static void
conn_connecting(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;
    struct conn *conn = carg;

    ASSERT(type == EVENT_CONN_CONNECTING);

    conn->connect_start = timer_now();
    stats->nconnect_issued++;
}

static void
conn_connected(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;
    struct conn *conn = carg;
    double connect_time;

    ASSERT(type == EVENT_CONN_CONNECTED);
    ASSERT(conn->connect_start > 0.0);
    ASSERT(timer_now() >= conn->connect_start);
    ASSERT(conn->connected);

    stats->nconnect++;

    connect_time = timer_now() - conn->connect_start;
    stats->connect_sum += connect_time;
    stats->connect_sum2 += SQUARE(connect_time);
    stats->connect_min = MIN(connect_time, stats->connect_min);
    stats->connect_max = MAX(connect_time, stats->connect_max);

    stats->nconn_active++;
    stats->nconn_active_max = MAX(stats->nconn_active, stats->nconn_active_max);
}

static void
conn_destroyed(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;
    struct conn *conn = carg;

    ASSERT(type == EVENT_CONN_DESTROYED);

    if (conn->connected) {
        double connection_time;

        ASSERT(stats->nconn_active > 0);
        stats->nconn_active--;

        connection_time = timer_now() - conn->connect_start;
        stats->connection_sum += connection_time;
        stats->connection_sum2 += SQUARE(connection_time);
        stats->connection_min = MIN(connection_time, stats->connection_min);
        stats->connection_max = MAX(connection_time, stats->connection_max);
    }
    stats->nconn_destroyed++;
}

static void
conn_timeout(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;

    ASSERT(type == EVENT_CONN_TIMEOUT);

    stats->nclient_timeout++;
}

static void
conn_failed(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;
    struct conn *conn = carg;

    ASSERT(type == EVENT_CONN_FAILED);
    ASSERT(conn->ctx == ctx);

    switch (conn->err) {
    case EMFILE:
        stats->nsock_fdunavail++;
        break;

    case ENFILE:
        stats->nsock_ftabfull++;
        break;

    case ECONNREFUSED:
        stats->nsock_refused++;
        break;

    case EPIPE:
    case ECONNRESET:
        stats->nsock_reset++;
        break;

    case ETIMEDOUT:
        stats->nsock_timedout++;
        break;

    case EADDRNOTAVAIL:
        stats->nsock_addrunavail++;
        break;

    default:
        stats->nsock_other_error++;
        break;
    }
}

static void
init(struct context *ctx, void *arg)
{
    ecb_register(ctx, EVENT_CONN_CREATED, conn_created, NULL);
    ecb_register(ctx, EVENT_CONN_CONNECTING, conn_connecting, NULL);
    ecb_register(ctx, EVENT_CONN_CONNECTED, conn_connected, NULL);
    ecb_register(ctx, EVENT_CONN_DESTROYED, conn_destroyed, NULL);
    ecb_register(ctx, EVENT_CONN_TIMEOUT, conn_timeout, NULL);
    ecb_register(ctx, EVENT_CONN_FAILED, conn_failed, NULL);
}

static void
no_op(struct context *ctx, void *arg)
{
    /* do nothing */
}

struct stats_collector conn_stats = {
    "collect connection related statistics",
    init,
    no_op,
    no_op,
    no_op
};
