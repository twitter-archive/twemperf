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
 * Return true if we are done issuing calls, otherwise
 * return false
 */
static bool
issue_call_done(struct context *ctx, struct conn *conn)
{
    if ((conn->ncall_created + conn->ncall_create_failed) ==
        ctx->opt.num_calls) {
        return true;
    }

    return false;
}

static int
issue_call(struct context *ctx, void *arg)
{
    struct conn *conn = arg;
    struct call *call;

    ASSERT(!issue_call_done(ctx, conn));

    call = call_get(conn);
    if (call == NULL) {
        conn->ncall_create_failed++;
        goto done;
    }

    call_make_req(ctx, call);

    /*
     * Enqueue call into sendq so that it can be sent later on
     * an out event
     */
    STAILQ_INSERT_TAIL(&conn->call_sendq, call, call_tqe);
    conn->ncall_sendq++;

    conn->ncall_created++;

    ecb_signal(ctx, EVENT_CALL_ISSUE_START, call);

done:
    if (issue_call_done(ctx, conn)) {
        log_debug(LOG_DEBUG, "issued %"PRIu32" %"PRIu32" of %"PRIu32" "
                  "calls on c %"PRIu64"", conn->ncall_create_failed,
                   conn->ncall_created, ctx->opt.num_calls, conn->id);
        if (conn->ncall_completed == conn->ncall_created) {
            ecb_signal(ctx, EVENT_CONN_DESTROYED, conn);
        }
        return -1;
    }

    log_debug(LOG_VERB, "issued %"PRIu32" %"PRIu32" of %"PRIu32" "
              "calls on c %"PRIu64"", conn->ncall_create_failed,
               conn->ncall_created, ctx->opt.num_calls, conn->id);

    return 0;
}

static void
destroyed(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct call *call = carg;
    struct conn *conn = call->conn;
    struct gen *g = &conn->call_gen;

    ASSERT(type == EVENT_CALL_DESTROYED);

    conn->ncall_completed++;

    if (issue_call_done(ctx, conn) &&
        (conn->ncall_completed == conn->ncall_created)) {

        log_debug(LOG_DEBUG, "completed %"PRIu32" of %"PRIu32" of %"PRIu32" "
                  "calls on c %"PRIu64"", conn->ncall_completed,
                  conn->ncall_created, ctx->opt.num_calls, conn->id);

        ecb_signal(ctx, EVENT_CONN_DESTROYED, conn);
        return;
    }

    log_debug(LOG_VERB, "completed %"PRIu32" of %"PRIu32" of %"PRIu32" "
              "calls on c %"PRIu64"", conn->ncall_completed,
              conn->ncall_created, ctx->opt.num_calls, conn->id);

    if (g->oneshot) {
        ecb_signal(ctx, EVENT_GEN_CALL_FIRE, g);
    }
}

static void
trigger(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct conn *conn = carg;
    struct gen *g = &conn->call_gen;
    struct dist_info *di = &ctx->call_dist;
    event_type_t firing_event = (di->type == DIST_NONE) ? EVENT_GEN_CALL_FIRE :
                                EVENT_INVALID;

    ASSERT(type == EVENT_GEN_CALL_TRIGGER);
    ASSERT(conn->ctx == ctx);

    gen_start(g, ctx, di, issue_call, conn, firing_event);
}

static void
init(struct context *ctx, void *arg)
{
    ecb_register(ctx, EVENT_CALL_DESTROYED, destroyed, NULL);
    ecb_register(ctx, EVENT_GEN_CALL_TRIGGER, trigger, NULL);
}

static void
no_op(struct context *ctx, void *arg)
{
    /* do nothing */
}

/*
 * Call generator is responsible for issuing and completing calls on
 * a given connection. A given connection can have multiple calls
 * outstanding on it.
 */
struct load_generator call_generator = {
    "issue calls on a connection at a given rate",
    init,
    no_op,
    no_op,
    no_op
};
