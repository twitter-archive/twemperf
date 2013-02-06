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
call_created(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    ASSERT(type == EVENT_CALL_CREATED);
}

static void
call_issue_start(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct call *call = carg;

    ASSERT(type == EVENT_CALL_ISSUE_START);

    call->req.issue_start = timer_now();
}

static void
call_send_start(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct call *call = carg;

    ASSERT(type == EVENT_CALL_SEND_START);
    ASSERT(call->req.issue_start > 0.0);

    call->req.send_start = timer_now();
}

static void
call_send_stop(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;
    struct call *call = carg;
    double req_xfer_time;

    ASSERT(type == EVENT_CALL_SEND_STOP);
    ASSERT(call->req.sent > 0);
    ASSERT(call->req.send_start > 0.0);
    ASSERT(call->req.send_start >= call->req.issue_start);

    call->req.send_stop = timer_now();

    stats->nreq++;

    stats->req_bytes_sent += call->req.sent;
    stats->req_bytes_sent2 += SQUARE(call->req.sent);
    stats->req_bytes_sent_min = MIN(call->req.sent, stats->req_bytes_sent_min);
    stats->req_bytes_sent_max = MAX(call->req.sent, stats->req_bytes_sent_max);

    req_xfer_time = timer_now() - call->req.send_start;
    stats->req_xfer_sum += req_xfer_time;
    stats->req_xfer_sum2 += SQUARE(req_xfer_time);
    stats->req_xfer_min = MIN(req_xfer_time, stats->req_xfer_min);
    stats->req_xfer_max = MAX(req_xfer_time, stats->req_xfer_max);
}

static void
call_recv_start(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;
    struct call *call = carg;
    double req_rsp_time;
    long int bin;

    ASSERT(type == EVENT_CALL_RECV_START);

    call->rsp.recv_start = timer_now();
    req_rsp_time = timer_now() - call->req.send_start;

    stats->req_rsp_sum += req_rsp_time;
    stats->req_rsp_sum2 += SQUARE(req_rsp_time);
    stats->req_rsp_min = MIN(req_rsp_time, stats->req_rsp_min);
    stats->req_rsp_max = MAX(req_rsp_time, stats->req_rsp_max);

    bin = MIN(lrint(req_rsp_time / HIST_BIN_WIDTH), HIST_NUM_BINS - 1);
    stats->req_rsp_hist[bin]++;
}

static void
call_recv_stop(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct stats *stats = &ctx->stats;
    struct call *call = carg;
    double rsp_xfer_time;

    ASSERT(type == EVENT_CALL_RECV_STOP);
    ASSERT(call->rsp.type < RSP_MAX_TYPES);

    stats->rsp_type[call->rsp.type]++;
    stats->nrsp++;

    stats->rsp_bytes_rcvd += call->rsp.rcvd;
    stats->rsp_bytes_rcvd2 += SQUARE(call->rsp.rcvd);
    stats->rsp_bytes_rcvd_min = MIN(call->rsp.rcvd, stats->rsp_bytes_rcvd_min);
    stats->rsp_bytes_rcvd_max = MAX(call->rsp.rcvd, stats->rsp_bytes_rcvd_max);

    rsp_xfer_time = timer_now() - call->rsp.recv_start;
    stats->rsp_xfer_sum += rsp_xfer_time;
    stats->rsp_xfer_sum2 += SQUARE(rsp_xfer_time);
    stats->rsp_xfer_min = MIN(rsp_xfer_time, stats->rsp_xfer_min);
    stats->rsp_xfer_max = MAX(rsp_xfer_time, stats->rsp_xfer_max);
}

static void
call_destroyed(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    ASSERT(type == EVENT_CALL_DESTROYED);
}

static void
init(struct context *ctx, void *arg)
{
    ecb_register(ctx, EVENT_CALL_CREATED, call_created, NULL);
    ecb_register(ctx, EVENT_CALL_ISSUE_START, call_issue_start, NULL);
    ecb_register(ctx, EVENT_CALL_SEND_START, call_send_start, NULL);
    ecb_register(ctx, EVENT_CALL_SEND_STOP, call_send_stop, NULL);
    ecb_register(ctx, EVENT_CALL_RECV_START, call_recv_start, NULL);
    ecb_register(ctx, EVENT_CALL_RECV_STOP, call_recv_stop, NULL);
    ecb_register(ctx, EVENT_CALL_DESTROYED, call_destroyed, NULL);
}

static void
no_op(struct context *ctx, void *arg)
{
    /* do nothing */
}

struct stats_collector call_stats = {
    "collect message related statistics",
    init,
    no_op,
    no_op,
    no_op
};
