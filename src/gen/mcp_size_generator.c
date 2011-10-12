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

static int
item_size_ticker(struct context *ctx, void *arg)
{
    struct dist_info *di = arg;

    di->next(di);

    return 0;
}

static void
trigger(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct gen *g = &ctx->size_gen;
    struct dist_info *di = &ctx->size_dist;

    ASSERT(type == EVENT_GEN_SIZE_TRIGGER);

    /*
     * A size generator can only be a oneshot generator. The only way to
     * tick this generator is by signalling the fire event.
     */
    gen_start(g, ctx, di, item_size_ticker, di, EVENT_GEN_SIZE_FIRE);
}

static void
init(struct context *ctx, void *arg)
{
    ecb_register(ctx, EVENT_GEN_SIZE_TRIGGER, trigger, NULL);
}

static void
no_op(struct context *ctx, void *arg)
{
    /* do nothing */
}

struct load_generator size_generator = {
    "generate item sizes",
    init,
    no_op,
    no_op,
    no_op
};
