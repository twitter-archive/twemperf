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

#ifndef _MCP_STATS_H_
#define _MCP_STATS_H_

#include <sys/resource.h>

#define HIST_MAX_TIME  100                     /* max time in sec (histogram resolution) */
#define HIST_BIN_WIDTH 1e-3                    /* bin width in sec (granularity) */
#define HIST_NUM_BINS  (HIST_MAX_TIME * 1000)  /* # bins */

struct stats {
    struct rusage rusage_start;                /* resource usage at start */
    struct rusage rusage_stop;                 /* resource usage at end */

    double        start_time;                  /* test start time in sec */
    double        stop_time;                   /* test end time in sec */

    uint32_t      nconn_created;               /* # connection created */
    uint32_t      nconn_destroyed;             /* # connection destroyed */

    uint32_t      nconn_active;                /* # connection active */
    uint32_t      nconn_active_max;            /* max # connection active */

    uint32_t      nconnect_issued;             /* # connect issued */
    uint32_t      nconnect;                    /* # successful connect */
    double        connect_sum;                 /* sum of connect time in sec */
    double        connect_sum2;                /* sum of connect time squared in sec^2 */
    double        connect_min;                 /* min connect time in sec */
    double        connect_max;                 /* max connect time in sec */
    double        connection_sum;              /* sum of connection time in sec */
    double        connection_sum2;             /* sum of connection time squared in sec^2 */
    double        connection_min;              /* min connection time in sec */
    double        connection_max;              /* max connection time in sec */

    uint32_t      nclient_timeout;             /* # client timeout */
    uint32_t      nsock_fdunavail;             /* # out of file descriptors */
    uint32_t      nsock_ftabfull;              /* # file table overflow */
    uint32_t      nsock_addrunavail;           /* # EADDRNOTAVAIL */
    uint32_t      nsock_refused;               /* # ECONNREFUSED */
    uint32_t      nsock_reset;                 /* # ECONNRESET or EPIPE */
    uint32_t      nsock_timedout;              /* # ETIMEDOUT */
    uint32_t      nsock_other_error;           /* # other errors */

    uint32_t      nreq;                        /* # request sent */
    double        req_bytes_sent;              /* request bytes sent */
    double        req_bytes_sent2;             /* request bytes sent squared */
    double        req_bytes_sent_min;          /* min request bytes sent */
    double        req_bytes_sent_max;          /* max request bytes sent */

    double        req_xfer_sum;                /* sum of request transfer time in sec */
    double        req_xfer_sum2;               /* sum of request transfer time squared in sec^2 */
    double        req_xfer_min;                /* min request transfer time in sec */
    double        req_xfer_max;                /* max request transfer time in sec */

    double        req_rsp_sum;                 /* sum of request to response time in sec */
    double        req_rsp_sum2;                /* sum of request to response time squared in sec^2 */
    double        req_rsp_min;                 /* min request to response time in sec */
    double        req_rsp_max;                 /* max request to response time in sec */
    long int      req_rsp_hist[HIST_NUM_BINS]; /* histogram of request to response time */

    uint32_t      nrsp;                        /* # responses received */
    double        rsp_bytes_rcvd;              /* bytes received */
    double        rsp_bytes_rcvd2;             /* bytes received squared */
    double        rsp_bytes_rcvd_min;          /* min bytes received */
    double        rsp_bytes_rcvd_max;          /* max bytes received */

    double        rsp_xfer_sum;                /* sum of response transfer sum */
    double        rsp_xfer_sum2;               /* sum of response transfer sum squared */
    double        rsp_xfer_min;                /* minimum response transfer time */
    double        rsp_xfer_max;                /* maximum response transfer time */

    uint32_t      rsp_type[RSP_MAX_TYPES];     /* # response type */
};

void stats_init(struct context *ctx);
void stats_start(struct context *ctx);
void stats_stop(struct context *ctx);
void stats_dump(struct context *ctx);

#endif
