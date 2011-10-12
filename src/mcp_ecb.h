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

#ifndef _MCP_ECB_H_
#define _MCP_ECB_H_

typedef void (*cb_t)(struct context *, event_type_t, void *, void *);

struct cb {
    cb_t  cb;    /* callback (function) */
    void  *rarg; /* registration arg (non-local name) */
    char  *name; /* name */
    char  *file; /* filename */
    int   line;  /* line */
};

#define MAX_NCB 4

struct action {
    int       ncb;           /* # callback */
    struct cb cb[MAX_NCB];   /* callback */
};

#define ecb_register(_ctx, _type, _cb, _rarg)   \
    _ecb_register(_ctx, _type, _cb, _rarg, #_cb, __FILE__, __LINE__)

#define ecb_signal(_ctx, _type, _carg)          \
    _ecb_signal(_ctx, _type, _carg);

void _ecb_register(struct context *ctx, event_type_t type, cb_t cb, void *rarg, char *name, char *file, int line);
void _ecb_signal(struct context *ctx, event_type_t type, void *carg);

#endif
