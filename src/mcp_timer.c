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

#include <sys/time.h>

#include <mcp_core.h>

/*
 * Timer wheel with TIMER_WHEEL_SIZE spokes. Each spoke represents a time
 * unit which equals TIMER_INTERVAL.
 *
 * With a 1 msec timer granularity and a wheel with 4096 spokes, it takes
 * ~4.1 seconds for the cursor to cycle back in single round.
 */
static struct timerhdr wheel[TIMER_WHEEL_SIZE]; /* timer wheel */
static uint32_t widx;                           /* current timer wheel idx */

static uint32_t nfree_timerq;                   /* # free timer q */
static struct timerhdr free_timerq;             /* free timer q */

static double now;                              /* current time in sec */
static double next_tick;                        /* next time to tick again in sec */

static uint64_t id;                             /* unique id */

static struct timer *
timer_get(void)
{
    struct timer *t;

    if (!LIST_EMPTY(&free_timerq)) {
        ASSERT(nfree_timerq > 0);

        t = LIST_FIRST(&free_timerq);
        nfree_timerq--;
        LIST_REMOVE(t, tle);
    } else {
        t = mcp_alloc(sizeof(*t));
        if (t == NULL) {
            return NULL;
        }
    }
    t->id = ++id;
    t->delta = 0;
    t->timeout = NULL;
    t->arg = NULL;

    log_debug(LOG_VERB, "get timer %p id %"PRIu64"", t, t->id);

    return t;
}

static void
timer_put(struct timer *t)
{
    log_debug(LOG_VERB, "put timer %p id %"PRIu64"", t, t->id);

    LIST_INSERT_HEAD(&free_timerq, t, tle);
    nfree_timerq++;
}

static void
timer_now_update(void)
{
    int status;
    struct timeval tv;

    status = gettimeofday(&tv, NULL);
    if (status < 0) {
        log_panic("gettimeofday failed: %s", strerror(errno));
    }

    now = TV_TO_SEC(&tv);
}

double
timer_now(void)
{
    return now;
}

void
timer_init(void)
{
    uint32_t i;

    for (i = 0; i < TIMER_WHEEL_SIZE; i++) {
        LIST_INIT(&wheel[i]);
    }
    widx = 0;

    nfree_timerq = 0;
    LIST_INIT(&free_timerq);

    timer_now_update();

    next_tick = timer_now() + TIMER_INTERVAL;
}

void
timer_deinit(void)
{
}

void
timer_tick(void)
{
    struct timer *t, *t_next; /* current and next timer */

    timer_now_update();

    while (timer_now() >= next_tick) {

        /* expire timed out timers in this slot */
        for (t = LIST_FIRST(&wheel[widx]); t != NULL && t->delta == 0;
             t = t_next) {
            t_next = LIST_NEXT(t, tle);

            log_debug(LOG_DEBUG, "fire timer %"PRIu64" '%s'", t->id, t->name);

            (t->timeout)(t, t->arg);

            LIST_REMOVE(t, tle);

            timer_put(t);
        }

        if (t != NULL) {
            t->delta--;
            log_debug(LOG_DEBUG, "decrementing timer %"PRIu64" '%s' delta to "
                      "%"PRIu64"", t->id, t->name, t->delta);
        }

        next_tick += TIMER_INTERVAL;
        widx = (widx + 1) % TIMER_WHEEL_SIZE;
    }
}

struct timer *
_timer_schedule(timeout_t timeout, void *arg, double delay, char *name)
{
    struct timerhdr *spoke;  /* spoke on the wheel */
    uint32_t sidx;           /* spoke index */
    struct timer *t, *t_new; /* current and new timer */
    uint64_t ticks, delta;
    double behind;

    t_new = timer_get();
    if (t_new == NULL) {
        return NULL;
    }
    t_new->timeout = timeout;
    t_new->arg = arg;
    t_new->name = name;

    behind = (timer_now() - next_tick);
    if (behind > 0.0) {
        delay += behind;
    }

    /* delay to ticks */
    ticks = (uint64_t)((delay + (TIMER_INTERVAL / 2.0)) * TIMER_TICKS_SEC);
    if (ticks == 0) {
        ticks = 1; /* minimum delay is a tick */
    }

    /* spoke for the new timer */
    sidx = (widx + ticks) % TIMER_WHEEL_SIZE;
    spoke = &wheel[sidx];

    /* # rounds around the wheel to expire the new timer */
    delta = ticks / TIMER_WHEEL_SIZE;

    for (t = LIST_FIRST(spoke); t != NULL && delta > t->delta;
         t = LIST_NEXT(t, tle)) {
        delta -= t->delta;
    }

    t_new->delta = delta;

    if (t == NULL) {
        LIST_INSERT_HEAD(spoke, t_new, tle);
    } else {
        LIST_INSERT_AFTER(t, t_new, tle);
    }

    if (LIST_NEXT(t_new, tle) != NULL) {
        LIST_NEXT(t_new, tle)->delta -= delta;
    }

    log_debug(LOG_DEBUG, "schedule timer %"PRIu64" '%s' to fire after %g s, "
              "%"PRIu64" ticks and %"PRIu64" rounds", t_new->id, t_new->name,
              delay, ticks, t_new->delta);

    return t_new;
}

void
_timer_cancel(struct timer *t)
{
    struct timer *t_next;

    log_debug(LOG_DEBUG, "cancel timer %"PRIu64" '%s'", t->id, t->name);

    t_next = LIST_NEXT(t, tle);
    if (t_next != NULL) {
        t_next->delta += t->delta;
    }

    LIST_REMOVE(t, tle);

    timer_put(t);
}
