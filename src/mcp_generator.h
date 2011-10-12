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

#ifndef _MCP_GENERATOR_H_
#define _MCP_GENERATOR_H_

typedef int (*gen_tick_t)(struct context *, void *);

/*
 * Generator ticks at a rate controlled by dist_info. On every
 * tick, invoke tick (gen_tick_t), and compute the next time
 * to tick if any timer was scheduled.
 */
struct gen {
    struct context    *ctx;        /* owner context */

    struct dist_info  *di;         /* dist info */

    struct timer      *timer;      /* ticking timer */
    char              *tickname;   /* tick who? */
    gen_tick_t        tick;        /* tick me, baby! */
    void              *arg;        /* opaque tick arg */
    double            start_time;  /* start time of a tick (const) */
    double            next_time;   /* next time to tick again */

    unsigned          oneshot:1;   /* one-shot? */
    unsigned          done:1;      /* done? */
};

#define gen_start(_g, _ctx, _di, _tick, _arg, _e)       \
    _gen_start(_g, _ctx, _di, #_tick, _tick, _arg, _e)

void _gen_start(struct gen *g, struct context *ctx, struct dist_info *di, char *tickname, gen_tick_t tick, void *arg, event_type_t firing_event);
void gen_stop(struct gen *g);

#endif
