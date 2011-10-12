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

#include <unistd.h>
#include <sys/epoll.h>

#include <mcp_core.h>

int
event_init(struct context *ctx, int size)
{
    int status, ep;
    struct epoll_event *event;

    ASSERT(ctx->nevent != 0);

    ep = epoll_create(size);
    if (ep < 0) {
        log_error("epoll create of size %d failed: %s", size, strerror(errno));
        return -1;
    }

    event = mcp_calloc(ctx->nevent, sizeof(*ctx->event));
    if (event == NULL) {
        status = close(ep);
        if (status < 0) {
            log_error("close e %d failed, ignored: %s", ep, strerror(errno));
        }
        log_error("epoll create of %d events failed: %s", ctx->nevent,
                  strerror(errno));
        return -1;
    }

    ctx->ep = ep;
    ctx->event = event;

    log_debug(LOG_DEBUG, "e %d with nevent %d timeout %d", ctx->ep,
              ctx->nevent, ctx->timeout);

    return 0;
}

void
event_deinit(struct context *ctx)
{
    int status;

    ASSERT(ctx->ep > 0);

    mcp_free(ctx->event);

    status = close(ctx->ep);
    if (status < 0) {
        log_error("close e %d failed, ignored: %s", ctx->ep, strerror(errno));
    }
    ctx->ep = -1;
}

int
event_add_out(int ep, struct conn *c)
{
    int status;
    struct epoll_event event;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);
    ASSERT(c->recv_active);

    if (c->send_active) {
        return 0;
    }

    //event.events = (uint32_t)(EPOLLIN | EPOLLOUT | EPOLLET);
    event.events = (uint32_t)(EPOLLIN | EPOLLOUT);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_MOD, c->sd, &event);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->send_active = 1;
    }

    return status;
}

int
event_del_out(int ep, struct conn *c)
{
    int status;
    struct epoll_event event;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);
    ASSERT(c->recv_active);

    if (!c->send_active) {
        return 0;
    }

    //event.events = (uint32_t)(EPOLLIN | EPOLLET);
    event.events = (uint32_t)(EPOLLIN);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_MOD, c->sd, &event);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->send_active = 0;
    }

    return status;
}

int
event_add_conn(int ep, struct conn *c)
{
    int status;
    struct epoll_event event;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);

    //event.events = (uint32_t)(EPOLLIN | EPOLLOUT | EPOLLET);
    event.events = (uint32_t)(EPOLLIN | EPOLLOUT);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_ADD, c->sd, &event);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->send_active = 1;
        c->recv_active = 1;
    }

    return status;
}

int
event_del_conn(int ep, struct conn *c)
{
    int status;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);

    status = epoll_ctl(ep, EPOLL_CTL_DEL, c->sd, NULL);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->recv_active = 0;
        c->send_active = 0;
    }

    return status;
}

int
event_wait(int ep, struct epoll_event *event, int nevent, int timeout)
{
    int nsd;

    ASSERT(ep > 0);
    ASSERT(event != NULL);
    ASSERT(nevent > 0);

    for (;;) {
        nsd = epoll_wait(ep, event, nevent, timeout);
        if (nsd > 0) {
            return nsd;
        }

        if (nsd == 0) {
            if (timeout == -1) {
               log_error("epoll wait on e %d with %d events and %d timeout "
                         "returned no events", ep, nevent, timeout);
                return -1;
            }

            return 0;
        }

        if (errno == EINTR) {
            continue;
        }

        log_error("epoll wait on e %d with %d events failed: %s", ep, nevent,
                  strerror(errno));

        return -1;
    }

    NOT_REACHED();
}
