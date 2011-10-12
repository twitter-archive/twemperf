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
gen_tick(struct timer *t, void *arg)
{
    struct gen *g = arg;
    struct context *ctx = g->ctx;
    struct dist_info *di = g->di;
    double now;

    ASSERT(g->timer == t);

    /* timer are freed by the timeout handler */
    g->timer = NULL;

    if (g->done) {
        gen_stop(g);
        return;
    }

    now = timer_now();

    while (now > g->next_time) {
        g->done = (g->tick(ctx, g->arg) < 0) ? 1 : 0;
        if (g->done) {
            gen_stop(g);
            return;
        }

        di->next(di);

        g->next_time += di->next_val;

        log_debug(LOG_DEBUG, "tick '%s' in %g s", g->tickname, di->next_val);
    }

    g->timer = timer_schedule(gen_tick, g, g->next_time - now);
}

static void
gen_fire(struct context *ctx, event_type_t type, void *rarg, void *carg)
{
    struct gen *g = carg;

    ASSERT(g->oneshot);

    if (g->done) {
        return;
    }

    g->done = (g->tick(ctx, g->arg) < 0) ? 1 : 0;
    if (g->done) {
        gen_stop(g);
    }
}

void
_gen_start(struct gen *g, struct context *ctx, struct dist_info *di,
           char *tickname, gen_tick_t tick, void *arg,
           event_type_t firing_event)
{
    g->ctx = ctx;

    g->di = di;

    /* g->timer is initialized later */
    g->tickname = tickname;
    g->tick = tick;
    g->arg = arg;
    g->start_time = timer_now();
    /* g->next_time is initialized later */

    /* g->done is initialized later */
    g->oneshot = (firing_event != EVENT_INVALID) ? 1 : 0;

    /*
     * Generators are either periodic or one-shot. A trigger event
     * is used to start a generator.
     *
     * Once a periodic generator is triggered, it ticks periodically
     * at a specfic rate controlled by dist_info. On every tick, it
     * invokes tick and schedules a timer to tick again.
     *
     * Once a one-shot generator is triggered, it can only tick by
     * explicitly signalling the firing event, at which point tick
     * is invoked.
     */
    if (g->oneshot) {
        ASSERT(firing_event < MAX_EVENT_TYPES);
        g->next_time = 0.0;
        g->timer = NULL;
        ecb_register(ctx, firing_event, gen_fire, NULL);
    } else {
        di->next(di);
        g->next_time = timer_now() + di->next_val;
        g->timer = timer_schedule(gen_tick, g, g->next_time - timer_now());
    }

    log_debug(LOG_DEBUG, "start gen %p to tick '%s'", g, g->tickname);

    g->done = (g->tick(ctx, g->arg) < 0) ? 1 : 0;
    if (g->done) {
        gen_stop(g);
    }
}

void
gen_stop(struct gen *g)
{
    ASSERT(g->done);

    if (g->timer != NULL) {
        timer_cancel(g->timer);
    }

    log_debug(LOG_DEBUG, "stop gen %p to tick '%s'", g, g->tickname);
}
