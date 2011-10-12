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

#ifndef _MCP_EVENT_H_
#define _MCP_EVENT_H_

/*
 * A hint to the kernel that is used to size the event backing store
 * of a given epoll instance
 */
#define EVENT_SIZE_HINT 1024

int event_init(struct context *ctx, int size);
void event_deinit(struct context *ctx);

int event_add_out(int ep, struct conn *c);
int event_del_out(int ep, struct conn *c);
int event_add_conn(int ep, struct conn *c);
int event_del_conn(int ep, struct conn *c);

int event_wait(int ep, struct epoll_event *event, int nevent, int timeout);

#endif
