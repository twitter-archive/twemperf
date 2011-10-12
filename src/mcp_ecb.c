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

void
_ecb_register(struct context *ctx, event_type_t type, cb_t cb, void *rarg,
              char *name, char *file, int line)
{
    struct action *act;
    struct cb *c, *end;

    ASSERT(type < MAX_EVENT_TYPES);

    act = &ctx->action[type];
    end = &act->cb[act->ncb];

    for (c = &act->cb[0]; c < end; c++) {
        /* do nothing for duplicate registration */
        if (c->cb == cb && c->rarg == rarg) {
            return;
        }
    }

    if (act->ncb >= MAX_NCB) {
        log_panic("attempted to register more than %d callbacks for event %d",
                  MAX_NCB, type);
    }

    log_debug(LOG_VERB, "register event %d at %d with cb '%s' from %s:%d", type,
              act->ncb, name, file, line);

    c = &act->cb[act->ncb++];

    c->cb = cb;
    c->rarg = rarg;

    c->name = name;
    c->file = file;
    c->line = line;
}

void
_ecb_signal(struct context *ctx, event_type_t type, void *carg)
{
    struct action *act;
    struct cb *c, *end;

    ASSERT(type < MAX_EVENT_TYPES);

    act = &ctx->action[type];
    end = &act->cb[act->ncb];

    /* signal registered events in FIFO order */
    for (c = &act->cb[0]; c < end; c++) {
        log_debug(LOG_VERB, "signal event %d at %d with cb '%s' from %s:%d",
                  type, c - act->cb, c->name, c->file, c->line);
        c->cb(ctx, type, c->rarg, carg);
    }
}
