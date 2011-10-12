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

#ifndef _MCP_TIMER_H_
#define _MCP_TIMER_H_

/*
 * We choose 1 msec as the timer granularity. So we must ensure that
 * timer_tick() is invoked at least once every 1 msec
 *
 * 1 tick = 1 msec
 * 1 sec = 1000 ticks
 */
#define TIMER_INTERVAL      (1.0 / 1000)            /* in sec */
#define TIMER_TICKS_SEC     (1.0 / TIMER_INTERVAL)  /* in ticks */

#define TIMER_WHEEL_SIZE    4096

struct timer;

typedef void (*timeout_t)(struct timer *t, void *arg);

struct timer {
    uint64_t          id;      /* unique id */
    LIST_ENTRY(timer) tle;     /* link in free q / timer wheel */
    uint64_t          delta;   /* delay to expiry as # rounds */
    timeout_t         timeout; /* timeout handler */
    void              *arg;    /* opaque data for timeout handler */
    char              *name;   /* timeout handler name */
};

LIST_HEAD(timerhdr, timer);

void timer_init(void);
void timer_deinit(void);

double timer_now(void);
void timer_tick(void);

#define timer_schedule(_timeout, _arg, _delay)              \
    _timer_schedule(_timeout, _arg, _delay, #_timeout)

#define timer_cancel(_timer) do {                           \
    _timer_cancel(_timer);                                  \
    _timer = NULL;                                          \
} while (0)

struct timer *_timer_schedule(timeout_t timeout, void *arg, double delay, char *name);
void _timer_cancel(struct timer *t);

#endif
