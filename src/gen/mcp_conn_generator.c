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

/*
 * Return true if we are done making connections, otherwise
 * return false
 */
static bool
make_conn_done(struct context *ctx)
{
    if ((ctx->nconn_created + ctx->nconn_create_failed) ==
        ctx->opt.num_conns) {
        return true;
    }

    return false;
}

static int
make_conn(struct context *ctx, void *arg)
{
    rstatus_t status;
    struct conn *conn;

    ASSERT(!make_conn_done(ctx));

    conn = conn_get(ctx);
    if (conn == NULL) {
        ctx->nconn_create_failed++;
        goto done;
    }

    status = core_connect(ctx, conn);
    if (status != MCP_OK) {
        ctx->nconn_create_failed++;
        ecb_signal(ctx, EVENT_CONN_FAILED, conn);
        goto done;
    }

    ctx->nconn_created++;
    ecb_signal(ctx, EVENT_CONN_CREATED, conn);

done:
    if (make_conn_done(ctx)) {
        log_debug(LOG_NOTICE, "created %"PRIu32" %"PRIu32" of %"PRIu32" "
                  "connections", ctx->nconn_create_failed, ctx->nconn_created,
                  ctx->opt.num_conns);
        if (ctx->nconn_destroyed == ctx->nconn_created) {
            core_stop(ctx);
        }
        return -1;
    }

    log_debug(LOG_INFO, "created %"PRIu32" %"PRIu32" of %"PRIu32" "
              "connections", ctx->nconn_create_failed, ctx->nconn_created,
              ctx->opt.num_conns);

    return 0;
}

static void
destroyed(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct conn *conn = carg;
    struct gen *g = &ctx->conn_gen;

    ASSERT(type == EVENT_CONN_DESTROYED);
    ASSERT(conn->ctx == ctx);

    core_close(ctx, conn);

    ctx->nconn_destroyed++;

    if (make_conn_done(ctx) && (ctx->nconn_destroyed == ctx->nconn_created)) {
        log_debug(LOG_NOTICE, "destroyed %"PRIu32" of %"PRIu32" of %"PRIu32" "
                  "connections", ctx->nconn_destroyed, ctx->nconn_created,
                  ctx->opt.num_conns);
        core_stop(ctx);
        return;
    }

    log_debug(LOG_INFO, "destroyed %"PRIu32" of %"PRIu32" of %"PRIu32" "
              "connections", ctx->nconn_destroyed, ctx->nconn_created,
              ctx->opt.num_conns);

    if (g->oneshot) {
        ecb_signal(ctx, EVENT_GEN_CONN_FIRE, g);
    }
}

static void
trigger(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct gen *g = &ctx->conn_gen;
    struct dist_info *di = &ctx->conn_dist;
    event_type_t firing_event = (di->type == DIST_NONE) ? EVENT_GEN_CONN_FIRE :
                                EVENT_INVALID;

    ASSERT(type == EVENT_GEN_CONN_TRIGGER);

    gen_start(g, ctx, di, make_conn, NULL, firing_event);
}

static void
init(struct context *ctx, void *arg)
{
    ecb_register(ctx, EVENT_CONN_DESTROYED, destroyed, NULL);
    ecb_register(ctx, EVENT_GEN_CONN_TRIGGER, trigger, NULL);
}

static void
no_op(struct context *ctx, void *arg)
{
    /* do nothing */
}

/*
 * Conn generator is responsible for creating and destroying connections
 * to a given server. A given server can have multiple connections
 * outstanding on it.
 */
struct load_generator conn_generator = {
    "creates connections to a server at a given rate",
    init,
    no_op,
    no_op,
    no_op
};
