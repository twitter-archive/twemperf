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

#include <stdlib.h>
#include <float.h>

#include <mcp_core.h>
#include <mcp_stats.h>

static void
stats_rusage_start(struct context *ctx)
{
    int status;
    struct stats *stats = &ctx->stats;

    status = getrusage(RUSAGE_SELF, &stats->rusage_start);
    if (status < 0) {
        log_panic("getrusage failed: %s", strerror(errno));
    }
}

static void
stats_rusage_stop(struct context *ctx)
{
    int status;
    struct stats *stats = &ctx->stats;

    status = getrusage(RUSAGE_SELF, &stats->rusage_stop);
    if (status < 0) {
        log_panic("getrusage failed: %s", strerror(errno));
    }
}

static void
stats_rusage_print(struct context *ctx)
{
    struct opt *opt = &ctx->opt;
    struct stats *stats = &ctx->stats;
    struct rusage *start, *stop;
    double delta, user, sys;
    long int maxrss, ixrss, idrss, isrss;
    long int minflt, majflt;
    long int nswap;
    long int inblock, oublock;
    long int msgsnd, msgrcv;
    long int nsignals;
    long int nvcsw, nivcsw;

    delta = stats->stop_time - stats->start_time;

    start = &stats->rusage_start;
    stop = &stats->rusage_stop;

    /* total amount of user time used */
    user = TV_TO_SEC(&stop->ru_utime) - TV_TO_SEC(&start->ru_utime);

    /* total amount of system time used */
    sys = TV_TO_SEC(&stop->ru_stime) - TV_TO_SEC(&start->ru_stime);

    log_stderr("CPU time [s]: user %.2f system %.2f (user %.1f%% system "
               "%.1f%% total %.1f%%)", user, sys, 100.0 * user / delta,
               100.0 * sys / delta, 100.0 * (user + sys) / delta);

    if (!opt->print_rusage) {
        return;
    }

    /* maximum resident set size (in kilobytes) */
    maxrss = stop->ru_maxrss - start->ru_maxrss;

    /*
     * Amount of sharing of text segment memory with other
     * processes (kilobyte-seconds)
     */
    ixrss = stop->ru_ixrss - start->ru_ixrss;

    /* amount of data segment memory used (kilobyte-seconds) */
    idrss = stop->ru_idrss - start->ru_idrss;

    /* amount of stack memory used (kilobyte-seconds) */
    isrss = stop->ru_isrss - start->ru_isrss;

    /*
     * Number of soft page faults (i.e. those serviced by reclaiming
     * a page from the list of pages awaiting reallocation
     */
    minflt = stop->ru_minflt - start->ru_minflt;

    /* number of hard page faults (i.e. those that required I/O) */
    majflt = stop->ru_majflt - start->ru_majflt;

    /* number of times a process was swapped out of physical memory  */
    nswap = stop->ru_nswap - start->ru_nswap;

    /* number of input operations via the file system */
    inblock = stop->ru_inblock - start->ru_inblock;

    /* number of output operations via the file system */
    oublock = stop->ru_oublock - start->ru_oublock;

    /* number of IPC messages sent */
    msgsnd = stop->ru_msgsnd - start->ru_msgsnd;

    /* number of IPC messages received */
    msgrcv = stop->ru_msgrcv - start->ru_msgrcv;

    /* number of signals delivered */
    nsignals = stop->ru_nsignals - start->ru_nsignals;

    /*
     * Number of voluntary context switches, i.e. because the process
     * gave up the process before it had to (usually to wait for some
     * resource to be available)
     */
    nvcsw = stop->ru_nvcsw - start->ru_nvcsw;

    /*
     * Number of involuntary context switches, i.e. a higher priority process
     * became runnable or the current process used up its time slice
     */
    nivcsw = stop->ru_nivcsw - start->ru_nivcsw;

    log_stderr("Maximum resident set size [KB]: %ld", maxrss);
    log_stderr("Text segment shared with other processes [KB-sec]: %ld", ixrss);
    log_stderr("Data segment used [KB-sec]: %ld", idrss);
    log_stderr("Stack memory used [KB-sec]: %ld", isrss);
    log_stderr("Number of soft page faults: %ld", minflt);
    log_stderr("Number of hard page faults: %ld", majflt);
    log_stderr("Number of times process was swapped out of physical memory: %ld", nswap);
    log_stderr("Number of input operations via file system: %ld", inblock);
    log_stderr("Number of output operations via file system: %ld", oublock);
    log_stderr("Number of IPC messages sent: %ld", msgsnd);
    log_stderr("Number of IPC messages received: %ld", msgrcv);
    log_stderr("Number of signals delivered: %ld", nsignals);
    log_stderr("Number of voluntary context switches: %ld", nvcsw);
    log_stderr("Number of involuntary context switches: %ld", nivcsw);
}

void
stats_init(struct context *ctx)
{
    struct stats *stats = &ctx->stats;
    uint32_t i;

    memset(&stats->rusage_start, 0, sizeof(stats->rusage_start));
    memset(&stats->rusage_stop, 0, sizeof(stats->rusage_stop));

    stats->start_time = 0.0;
    stats->stop_time = 0.0;

    stats->nconn_created = 0;
    stats->nconn_destroyed = 0;

    stats->nconn_active = 0;
    stats->nconn_active_max = 0;

    stats->nconnect_issued = 0;
    stats->nconnect = 0;
    stats->connect_sum = 0.0;
    stats->connect_sum2 = 0.0;
    stats->connect_min = DBL_MAX;
    stats->connect_max = 0.0;
    stats->connection_sum = 0.0;
    stats->connection_sum2 = 0.0;
    stats->connection_min = DBL_MAX;
    stats->connection_max = 0.0;

    stats->nclient_timeout = 0;
    stats->nsock_fdunavail = 0;
    stats->nsock_ftabfull = 0;
    stats->nsock_addrunavail = 0;
    stats->nsock_refused = 0;
    stats->nsock_reset = 0;
    stats->nsock_timedout = 0;
    stats->nsock_other_error = 0;

    stats->nreq = 0;
    stats->req_bytes_sent = 0.0;
    stats->req_bytes_sent2 = 0.0;
    stats->req_bytes_sent_min = DBL_MAX;
    stats->req_bytes_sent_max = 0.0;

    stats->req_xfer_sum = 0.0;
    stats->req_xfer_sum2 = 0.0;
    stats->req_xfer_min = DBL_MAX;
    stats->req_xfer_max = 0.0;

    stats->req_rsp_sum = 0.0;
    stats->req_rsp_sum2 = 0.0;
    stats->req_rsp_min = DBL_MAX;
    stats->req_rsp_max = 0.0;
    for (i = 0; i < HIST_NUM_BINS; i++) {
        stats->req_rsp_hist[i] = 0;
    }

    stats->nrsp = 0;
    stats->rsp_bytes_rcvd = 0.0;
    stats->rsp_bytes_rcvd2 = 0.0;
    stats->rsp_bytes_rcvd_min = DBL_MAX;
    stats->rsp_bytes_rcvd_max = 0.0;

    stats->rsp_xfer_sum = 0.0;
    stats->rsp_xfer_sum2 = 0.0;
    stats->rsp_xfer_min = DBL_MAX;
    stats->rsp_xfer_max = 0.0;

    for (i = 0; i < RSP_MAX_TYPES; i++) {
        stats->rsp_type[i] = 0;
    }
}

void
stats_start(struct context *ctx)
{
    struct stats *stats = &ctx->stats;

    stats_rusage_start(ctx);
    stats->start_time = timer_now();
}

void
stats_stop(struct context *ctx)
{
    struct stats *stats = &ctx->stats;

    stats_rusage_stop(ctx);
    stats->stop_time = timer_now();
}

void
stats_dump(struct context *ctx)
{
    struct opt *opt = &ctx->opt;
    struct stats *stats = &ctx->stats;
    double conn_period, conn_rate;
    double conn_avg, conn_min, conn_max, conn_stddev;
    double connection_avg, connection_min, connection_max, connection_stddev;
    double req_rate, req_period;
    double req_size_avg, req_size_min, req_size_max, req_size_stddev;
    double rsp_rate, rsp_period;
    double rsp_size_avg, rsp_size_min, rsp_size_max, rsp_size_stddev;
    double req_rsp_avg, req_rsp_min, req_rsp_max, req_rsp_stddev;
    double req_rsp_p25 = 0.0, req_rsp_p50 = 0.0, req_rsp_p75 = 0.0;
    double req_rsp_p95 = 0.0, req_rsp_p99 = 0.0, req_rsp_p999 = 0.0;
    double total_size, total_rate;
    double delta;
    double bin_time;
    uint32_t i, nerror;
    long int n;

    /* stop stats collection */
    stats_stop(ctx);

    ASSERT(stats->stop_time > stats->start_time);

    delta = stats->stop_time - stats->start_time;

    /*
     * Total Section
     * 1. number of successful TCP connections
     * 2. number of requests
     * 3. number of responses
     * 4. overall time spent testing
     */
    log_stderr("");
    log_stderr("Total: connections %"PRIu32" requests %"PRIu32" responses "
               "%"PRIu32" test-duration %.3f s", stats->nconnect_issued,
               stats->nreq, stats->nrsp, delta);

    /*
     * Connection Section
     * 1. rate at which new connections were initiated
     * 2. time period between any two successive connections, and
     * 3. number of concurrent connections open to the server
     * 4. connection time for successful connections
     * 5. connect time for successful connections
     */
    if (stats->nconnect_issued != 0) {
        log_stderr("");

        conn_period = delta / stats->nconnect_issued;
        conn_rate = stats->nconnect_issued / delta;

        log_stderr("Connection rate: %.1f conn/s (%.1f ms/conn <= %"PRIu32" "
                   "concurrent connections)", conn_rate, 1e3 * conn_period,
                   stats->nconn_active_max);

        connection_avg = stats->connection_sum / stats->nconnect;
        connection_min = stats->connection_min;
        connection_max = stats->connection_max;
        connection_stddev = STDDEV(stats->connection_sum, stats->connection_sum2,
                                   stats->nconnect);
        log_stderr("Connection time [ms]: avg %.1f min %.1f max %.1f stddev "
                   "%.2f", 1e3 * connection_avg, 1e3 * connection_min,
                   1e3 * connection_max, 1e3 * connection_stddev);

        conn_avg = stats->connect_sum / stats->nconnect;
        conn_min = stats->connect_min;
        conn_max = stats->connect_max;
        conn_stddev = STDDEV(stats->connect_sum, stats->connect_sum2,
                             stats->nconnect);
        log_stderr("Connect time [ms]: avg %.1f min %.1f max %.1f "
                   "stddev %.2f", 1e3 * conn_avg, 1e3 * conn_min,
                   1e3 * conn_max, 1e3 * conn_stddev);
    }

    /*
     * Request section
     * 1. request rate - rate at which request were issued
     * 2. request size
     */
    if (stats->nreq != 0) {
        log_stderr("");

        req_period = delta / stats->nreq;
        req_rate = stats->nreq / delta;

        log_stderr("Request rate: %.1f req/s (%.1f ms/req)", req_rate,
                   1e3 * req_period);

        req_size_min = stats->req_bytes_sent_min;
        req_size_avg = stats->req_bytes_sent / stats->nreq;
        req_size_max = stats->req_bytes_sent_max;
        req_size_stddev = STDDEV(stats->req_bytes_sent, stats->req_bytes_sent2,
                                 stats->nreq);

        log_stderr("Request size [B]: avg %.1f min %.1f max %.1f stddev %.2f",
                   req_size_avg, req_size_min, req_size_max, req_size_stddev);
    }

    /*
     * Response section
     * 1. response rate
     * 2. response size
     * 3. response time - how long it took for the server to respond
     * 4. response types
     */
    if (stats->nrsp != 0) {
        log_stderr("");

        rsp_period = delta / stats->nrsp;
        rsp_rate = stats->nrsp / delta;

        log_stderr("Response rate: %.1f rsp/s (%.1f ms/rsp)", rsp_rate,
                   1e3 * rsp_period);

        rsp_size_min = stats->rsp_bytes_rcvd_min;
        rsp_size_avg = stats->rsp_bytes_rcvd / stats->nrsp;
        rsp_size_max = stats->rsp_bytes_rcvd_max;
        rsp_size_stddev = STDDEV(stats->rsp_bytes_rcvd, stats->rsp_bytes_rcvd2,
                                 stats->nrsp);

        log_stderr("Response size [B]: avg %.1f min %.1f max %.1f stddev %.2f",
                   rsp_size_avg, rsp_size_min, rsp_size_max, rsp_size_stddev);

        req_rsp_avg = stats->req_rsp_sum / stats->nrsp;
        req_rsp_min = stats->req_rsp_min;
        req_rsp_max = stats->req_rsp_max;
        req_rsp_stddev = STDDEV(stats->req_rsp_sum, stats->req_rsp_sum2,
                                stats->nrsp);

        log_stderr("Response time [ms]: avg %.1f min %.1f max %.1f stddev %.2f",
                   1e3 * req_rsp_avg, 1e3 * req_rsp_min, 1e3 * req_rsp_max,
                   req_rsp_stddev);

        if (opt->print_histogram) {
            log_stderr("Response time histogram [ms]:");

            for (i = 0; i < HIST_NUM_BINS; i++) {
                if (stats->req_rsp_hist[i] == 0) {
                    continue;
                }
                if (i > 0 && stats->req_rsp_hist[i - 1] == 0) {
                    log_stderr("%14c", ':');
                }
                bin_time = i * HIST_BIN_WIDTH;
                log_stderr("%16.1f %u", 1e3 * bin_time, stats->req_rsp_hist[i]);
            }
            if (stats->req_rsp_hist[i - 1] == 0) {
                log_stderr("%14c", ':');
            }
        }

        for (i = 0, n = 0; i < HIST_NUM_BINS; i++) {
            n += stats->req_rsp_hist[i];

            if (req_rsp_p25 == 0.0 && n >= lrint(0.25 * stats->nrsp)) {
                req_rsp_p25 = i * HIST_BIN_WIDTH;
            }

            if (req_rsp_p50 == 0.0 && n >= lrint(0.50 * stats->nrsp)) {
                req_rsp_p50 = i * HIST_BIN_WIDTH;
            }

            if (req_rsp_p75 == 0.0 && n >= lrint(0.70 * stats->nrsp)) {
                req_rsp_p75 = i * HIST_BIN_WIDTH;
            }

            if (req_rsp_p95 == 0.0 && n >= lrint(0.95 * stats->nrsp)) {
                req_rsp_p95 = i * HIST_BIN_WIDTH;
            }

            if (req_rsp_p99 == 0.0 && n >= lrint(0.99 * stats->nrsp)) {
                req_rsp_p99 = i * HIST_BIN_WIDTH;
            }

            if (req_rsp_p999 == 0.0 && n >= lrint(0.999 * stats->nrsp)) {
                req_rsp_p999 = i * HIST_BIN_WIDTH;
            }
        }

        log_stderr("Response time [ms]: p25 %.1f p50 %.1f p75 %.1f",
                   1e3 * req_rsp_p25, 1e3 * req_rsp_p50, 1e3 * req_rsp_p75);

        log_stderr("Response time [ms]: p95 %.1f p99 %.1f p999 %.1f",
                   1e3 * req_rsp_p95, 1e3 * req_rsp_p99, 1e3 * req_rsp_p999);

        log_stderr("Response type: stored %"PRIu32" not_stored %"PRIu32" "
                   "exists %"PRIu32" not_found %"PRIu32"",
                   stats->rsp_type[RSP_STORED], stats->rsp_type[RSP_NOT_STORED],
                   stats->rsp_type[RSP_EXISTS], stats->rsp_type[RSP_NOT_FOUND]);

        log_stderr("Response type: num %"PRIu32" deleted %"PRIu32" end %"PRIu32
                   " value %"PRIu32"", stats->rsp_type[RSP_NUM],
                   stats->rsp_type[RSP_DELETED], stats->rsp_type[RSP_END],
                   stats->rsp_type[RSP_VALUE]);

        log_stderr("Response type: error %"PRIu32" client_error %"PRIu32" "
                   "server_error %"PRIu32"", stats->rsp_type[RSP_ERROR],
                   stats->rsp_type[RSP_CLIENT_ERROR],
                   stats->rsp_type[RSP_SERVER_ERROR]);
    }

    /*
     * Error section
     */
    log_stderr("");

    nerror = stats->nclient_timeout + stats->nsock_fdunavail +
             stats->nsock_ftabfull + stats->nsock_addrunavail +
             stats->nsock_refused + stats->nsock_reset +
             stats->nsock_timedout + stats->nsock_other_error;

    log_stderr("Errors: total %"PRIu32" client-timo %"PRIu32" "
               "socket-timo %"PRIu32" connrefused %"PRIu32" "
               "connreset %"PRIu32"", nerror, stats->nclient_timeout,
               stats->nsock_timedout, stats->nsock_refused,
               stats->nsock_reset);

    log_stderr("Errors: fd-unavail %"PRIu32" ftab-full %"PRIu32" "
               "addrunavail %"PRIu32" other %"PRIu32"",
               stats->nsock_fdunavail, stats->nsock_ftabfull,
               stats->nsock_addrunavail, stats->nsock_other_error);

    /*
     * Resource usage section
     * 1. CPU utilization
     * 2. Net I/O
     */
    log_stderr("");

    stats_rusage_print(ctx);

    if (stats->nreq + stats->nrsp != 0) {
        double total;
        char *metric;

        total_size = stats->req_bytes_sent + stats->rsp_bytes_rcvd;
        total_rate = total_size / delta;

        if (total_size <= KB) {
            total = total_size;
            metric = "B";
        } else if (total_size <= MB) {
            total = total_size / KB;
            metric = "KB";
        } else if (total_size <= GB) {
            total = total_size / MB;
            metric = "MB";
        } else {
            total = total_size / GB;
            metric = "GB";
        }

        log_stderr("Net I/O: bytes %.1f %s rate %.1f KB/s (%.1f*10^6 bps)",
                   total, metric, total_rate / 1024.0,
                   8e-6 * total_size / delta);
    }

    log_stderr("");

    exit(0);
}
