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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <mcp_core.h>

extern struct string req_strings[];

static int show_help;
static int show_version;

#define MCP_LOG_DEFAULT      LOG_NOTICE
#define MCP_LOG_MIN          LOG_EMERG
#define MCP_LOG_MAX          LOG_PVERB
#define MCP_LOG_PATH         "stderr"

#define MCP_SERVER           "localhost"
#define MCP_PORT             11211

#define MCP_CLIENT_ID        0
#define MCP_CLIENT_N         1

#define MCP_METHOD_STR       "set"
#define MCP_METHOD           REQ_SET

#define MCP_EXPIRY_STR       "0"
#define MCP_EXPIRY           0

#define MCP_PREFIX           "mcp:"
#define MCP_PREFIX_LEN       CALL_PREFIX_LEN

#define MCP_TIMEOUT          0.0
#define MCP_TIMEOUT_STR      "0.0"

#define MCP_LINGER_STR       "off"
#define MCP_LINGER           0

#define MCP_SEND_BUFSIZE     4096
#define MCP_RECV_BUFSIZE     16384

#define MCP_NUM_CONNS        1
#define MCP_NUM_CALLS        1

#define MCP_CONN_DIST_STR    "0"
#define MCP_CONN_DIST        DIST_NONE
#define MCP_CONN_DIST_MIN    0.0
#define MCP_CONN_DIST_MAX    0.0

#define MCP_CALL_DIST_STR    "0"
#define MCP_CALL_DIST        DIST_NONE
#define MCP_CALL_DIST_MIN    0.0
#define MCP_CALL_DIST_MAX    0.0

#define MCP_SIZE_DIST_STR    "d1"
#define MCP_SIZE_DIST        DIST_DETERMINISTIC
#define MCP_SIZE_DIST_MIN    1
#define MCP_SIZE_DIST_MAX    1

#define MCP_PRINT_RUSAGE     0

static struct option long_options[] = {
    { "help",               no_argument,        NULL,   'h' },
    { "version",            no_argument,        NULL,   'V' },
    { "verbosity",          required_argument,  NULL,   'v' },
    { "output",             required_argument,  NULL,   'o' },
    { "server",             required_argument,  NULL,   's' },
    { "port",               required_argument,  NULL,   'p' },
    { "print-histogram",    no_argument,        NULL,   'H' },
    { "timeout",            required_argument,  NULL,   't' },
    { "linger",             required_argument,  NULL,   'l' },
    { "send-buffer",        required_argument,  NULL,   'b' },
    { "recv-buffer",        required_argument,  NULL,   'B' },
    { "disable-nodelay",    no_argument,        NULL,   'D' },
    { "method",             required_argument,  NULL,   'm' },
    { "expiry",             required_argument,  NULL,   'e' },
    { "use-noreply",        no_argument,        NULL,   'q' },
    { "prefix",             required_argument,  NULL,   'P' },
    { "client",             required_argument,  NULL,   'c' },
    { "num-conns",          required_argument,  NULL,   'n' },
    { "num-calls",          required_argument,  NULL,   'N' },
    { "conn-rate",          required_argument,  NULL,   'r' },
    { "call-rate",          required_argument,  NULL,   'R' },
    { "sizes",              required_argument,  NULL,   'z' },
    { NULL,                 0,                  NULL,    0  }
};

static char short_options[] = "hVv:o:s:p:Ht:l:b:B:Dm:e:qP:c:n:N:r:R:z:";

static void
mcp_show_usage(void)
{
    log_stderr(
        "Usage: mcperf [-?hV] [-v verbosity level] [-o output file]" CRLF
        "              [-s server] [-p port] [-H] [-t timeout] [-l linger]" CRLF
        "              [-b send-buffer] [-B recv-buffer] [-D]" CRLF
        "              [-m method] [-e expiry] [-q] [-P prefix]" CRLF
        "              [-c client] [-n num-conns] [-N num-calls]" CRLF
        "              [-r conn-rate] [-R call-rate] [-z sizes]" CRLF
        "" CRLF
        "Options:" CRLF
        "  -h, --help            : this help" CRLF
        "  -V, --version         : show version and exit" CRLF
        "  -v, --verbosity=N     : set logging level (default: %d, min: %d, max: %d)" CRLF
        "  -o, --output=S        : set logging file (default: %s)" CRLF
        "  -s, --server=S        : set the hostname of the server (default: %s)" CRLF
        "  -p, --port=N          : set the port number of the server (default: %d)" CRLF
        "  -H, --print-histogram : print response time histogram" CRLF
        "  ...",
        MCP_LOG_DEFAULT, MCP_LOG_MIN, MCP_LOG_MAX, MCP_LOG_PATH,
        MCP_SERVER, MCP_PORT);

    log_stderr(
        "  -t, --timeout=X       : set the connection and response timeout in sec (default: %s sec)" CRLF
        "  -l, --linger=N        : set the linger timeout in sec, when closing TCP connections (default: %s)" CRLF
        "  -b, --send-buffer=N   : set socket send buffer size (default: %d bytes)" CRLF
        "  -B, --recv-buffer=N   : set socket recv buffer size (default: %d bytes)" CRLF
        "  -D, --disable-nodelay : disable tcp nodelay" CRLF
        "  ...",
        MCP_TIMEOUT_STR, MCP_LINGER_STR,
        MCP_SEND_BUFSIZE, MCP_RECV_BUFSIZE
        );

    log_stderr(
        "  -m, --method=M        : set the method to use when issuing memcached request (default: %s)" CRLF
        "  -e, --expiry=N        : set the expiry value in sec for generated requests (default: %s sec)" CRLF
        "  -q, --use-noreply     : set noreply for generated requests" CRLF
        "  -P, --prefix=S        : set the prefix of generated keys (default: %s)" CRLF
        "  ...",
        MCP_METHOD_STR, MCP_EXPIRY_STR,
        MCP_PREFIX
        );

    log_stderr(
        "  -c, --client=I/N      : set mcperf instance to be I out of total N instances (default: %d/%d)" CRLF
        "  -n, --num-conns=N     : set the number of connections to create (default: %d)" CRLF
        "  -N, --num-calls=N     : set the number of calls to create on each connection (default: %d)" CRLF
        "  -r, --conn-rate=R     : set the connection creation rate (default: %s conns/sec) "CRLF
        "  -R, --call-rate=R     : set the call creation rate (default: %s calls/sec)" CRLF
        "  -z, --sizes=R         : set the distribution for item sizes (default: %s bytes)" CRLF
        "  ...",
        MCP_CLIENT_ID, MCP_CLIENT_N, MCP_NUM_CONNS, MCP_NUM_CALLS,
        MCP_CONN_DIST_STR, MCP_CALL_DIST_STR, MCP_SIZE_DIST_STR
        );

    log_stderr(
        "Where:" CRLF
        "  N is an integer" CRLF
        "  X is a real" CRLF
        "  S is a string" CRLF
        "  M is a method string and is either a 'get', 'gets', 'delete', 'cas', 'set', 'add', 'replace'" CRLF
        "  'append', 'prepend', 'incr', 'decr'" CRLF
        "  R is the rate written as [D]R1[,R2] where:" CRLF
        "  D is the distribution type and is either deterministic 'd', uniform 'u', or exponential 'e' and if:" CRLF
        "  D is ommited or set to 'd', a deterministic interval specified by parameter R1 is used" CRLF
        "  D is set to 'e', an exponential distibution with mean interval of R1 is used" CRLF
        "  D is set to 'u', a uniform distribution over interval [R1, R2) is used" CRLF
        "  R is 0, the next request or connection is created after the previous one completes" CRLF
        "  "
        );
}

static void
mcp_set_default_options(struct context *ctx)
{
    struct opt *opt = &ctx->opt;

    opt->log_level = MCP_LOG_DEFAULT;
    opt->log_filename = NULL;

    /* default server info */
    opt->server = MCP_SERVER;
    opt->port = MCP_PORT;
    memset(&opt->si, 0, sizeof(opt->si));

    opt->print_histogram = 0;

    opt->timeout = MCP_TIMEOUT;
    /* opt->linger_timeout is don't-care when lingering is off */
    opt->linger = MCP_LINGER;
    opt->send_buf_size = MCP_SEND_BUFSIZE;
    opt->recv_buf_size = MCP_RECV_BUFSIZE;
    opt->disable_nodelay = 0;

    opt->method = MCP_METHOD;
    opt->expiry = MCP_EXPIRY;
    opt->use_noreply = 0;
    opt->prefix.data = MCP_PREFIX;
    opt->prefix.len = sizeof(MCP_PREFIX) - 1;

    /* default client id */
    opt->client.id = MCP_CLIENT_ID;
    opt->client.n = MCP_CLIENT_N;

    /* default connection generator */
    opt->num_conns = MCP_NUM_CONNS;
    opt->conn_dopt.type = MCP_CONN_DIST;
    opt->conn_dopt.min = MCP_CONN_DIST_MIN;
    opt->conn_dopt.max = MCP_CONN_DIST_MAX;

    /* default call generator */
    opt->num_calls = MCP_NUM_CALLS;
    opt->call_dopt.type = MCP_CALL_DIST;
    opt->call_dopt.min = MCP_CALL_DIST_MIN;
    opt->call_dopt.max = MCP_CALL_DIST_MAX;

    /* default size generator */
    opt->size_dopt.type = MCP_SIZE_DIST;
    opt->size_dopt.min = MCP_SIZE_DIST_MIN;
    opt->size_dopt.max = MCP_SIZE_DIST_MAX;

    opt->print_rusage = MCP_PRINT_RUSAGE;
}

static rstatus_t
mcp_get_dist_opt(struct dist_opt *dopt, char *line)
{
    char *pos;

    /*
     * Parse any distribution value specified as:
     *   --distribution [d|u|e]T1[,T2]
     */
    switch (*line) {
    case 'd':
        dopt->type = DIST_DETERMINISTIC;
        line++;
        break;

    case 'u':
        dopt->type = DIST_UNIFORM;
        line++;
        break;

    case 'e':
        dopt->type = DIST_EXPONENTIAL;
        line++;
        break;

    case 's':
        dopt->type = DIST_SEQUENTIAL;
        line++;
        break;

    default:
        dopt->type = DIST_NONE;
        break;
    }
    dopt->min = 0.0;
    dopt->max = 0.0;

    switch (dopt->type) {
    case DIST_NONE:
        dopt->min = mcp_atod(line);
        if (dopt->min < 0.0) {
            log_stderr("mcperf: invalid distribution value '%s'", line);
            return MCP_ERROR;
        }
        if (dopt->min != 0.0) {
            dopt->type = DIST_DETERMINISTIC;
            dopt->min = 1.0 / dopt->min;
            dopt->max = dopt->min;
        }
        break;

    case DIST_DETERMINISTIC:
    case DIST_EXPONENTIAL:
    case DIST_SEQUENTIAL:
        dopt->min = mcp_atod(line);
        if (dopt->min <= 0.0) {
            log_stderr("mcperf: invalid mean value '%s'", line);
            return MCP_ERROR;
        }
        dopt->max = dopt->min;
        break;

    case DIST_UNIFORM:
        pos = strchr(line, ',');
        if (pos == NULL) {
            log_stderr("mcperf: invalid uniform distribution value '%s'", line);
            return MCP_ERROR;
        }
        *pos = '\0';

        dopt->min = mcp_atod(line);
        if (dopt->min <= 0.0) {
            log_stderr("mcperf: invalid minimum value '%s'", line);
            return MCP_ERROR;
        }

        line = pos + 1;

        dopt->max = mcp_atod(line);
        if (dopt->max <= 0.0) {
            log_stderr("mcperf: invalid maximum value '%s'", line);
            return MCP_ERROR;
        }
        if (dopt->max < dopt->min) {
            log_stderr("mcperf: maximum value '%g' should be greater than or "
                       "equal to minium value '%g'", dopt->max, dopt->min);
            return MCP_ERROR;
        }

        break;

    default:
        NOT_REACHED();
    }

    return MCP_OK;
}

static rstatus_t
mcp_get_method(struct context *ctx, char *line)
{
    struct opt *opt = &ctx->opt;
    size_t linelen = strlen(line);
    struct string *str;

    for (str = req_strings; str->data != NULL; str++) {
        if ((str->len - 1 == linelen) &&
            strncmp(str->data, line, linelen) == 0) {
            opt->method = str - req_strings;
            return MCP_OK;
        }
    }

    log_stderr("mcperf: '%s' is an invalid method; valid methods are get, "
               "gets, delete, cas, set, add, replace, prepend, incr and "
               "decr", line);

    return MCP_ERROR;
}

static rstatus_t
mcp_get_options(struct context *ctx, int argc, char **argv)
{
    rstatus_t status;
    struct opt *opt;
    int c, value;
    size_t size;
    double real;
    char *pos;

    opt = &ctx->opt;
    opterr = 0;

    for (;;) {
        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            /* no more options */
            break;
        }

        switch(c) {
        case 'h':
            show_version = 1;
            show_help = 1;
            break;

        case 'V':
            show_version = 1;
            break;

        case 'v':
            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: option -v requires a number");
                return MCP_ERROR;
            }
            opt->log_level = value;
            break;

        case 'o':
            opt->log_filename = optarg;
            break;

        case 's':
            opt->server = optarg;
            break;

        case 'p':
            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: option -p requires a number");
                return MCP_ERROR;
            }
            if (!mcp_valid_port(value)) {
                log_stderr("mcperf: option -p value %d is not a valid port",
                            value);
                return MCP_ERROR;
            }

            opt->port = (uint16_t)value;
            break;

        case 'H':
            opt->print_histogram = 1;
            break;

        case 't':
            real = mcp_atod(optarg);
            if (real < 0.0) {
                log_stderr("mcperf: option -T requires a real number");
                return MCP_ERROR;
            }
            opt->timeout = real;
            break;

        case 'l':
            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: option -l requires a number");
                return MCP_ERROR;
            }
            opt->linger = 1;
            opt->linger_timeout = value;
            break;

        case 'b':
            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: option -b requires a number");
                return MCP_ERROR;
            }
            opt->send_buf_size = value;
            break;

        case 'B':
            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: option -B requires a number");
                return MCP_ERROR;
            }
            opt->recv_buf_size = value;
            break;

        case 'm':
            status = mcp_get_method(ctx, optarg);
            if (status != MCP_OK) {
                return status;
            }
            break;

        case 'e':
            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: option -e requires a number");
                return MCP_ERROR;
            }
            opt->expiry = (uint32_t)value;
            break;

        case 'q':
            opt->use_noreply = 1;
            break;

        case 'P':
            size = strlen(optarg);
            if (size > MCP_PREFIX_LEN) {
                log_stderr("mcperf: key prefix cannot exceed %d in length",
                           MCP_PREFIX_LEN);
                return MCP_ERROR;
            }
            opt->prefix.data = optarg;
            opt->prefix.len = size;
            break;

        case 'c':
            pos = strchr(optarg, '/');
            if (pos == NULL) {
                log_stderr("mcperf: invalid client id format '%s'", optarg);
                return MCP_ERROR;
            }
            *pos = '\0';

            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: client id is not a number '%s'", optarg);
                return MCP_ERROR;
            }
            opt->client.id = (uint32_t)value;

            optarg = pos + 1;

            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: number of clients is not a number '%s'",
                           optarg);
                return MCP_ERROR;
            }
            opt->client.n = (uint32_t)value;
            break;

        case 'n':
            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: option -n requires a number");
                return MCP_ERROR;
            }
            opt->num_conns = (uint32_t)value;
            break;

        case 'N':
            value = mcp_atoi(optarg);
            if (value < 0) {
                log_stderr("mcperf: option -N requires a number");
                return MCP_ERROR;
            }
            opt->num_calls = (uint32_t)value;
            break;

        case 'r':
            status = mcp_get_dist_opt(&opt->conn_dopt, optarg);
            if (status != MCP_OK) {
                return status;
            }
            break;

        case 'R':
            status = mcp_get_dist_opt(&opt->call_dopt, optarg);
            if (status != MCP_OK) {
                return status;
            }
            break;

        case 'D':
            opt->disable_nodelay = 1;
            break;

        case 'z':
            status = mcp_get_dist_opt(&opt->size_dopt, optarg);
            if (status != MCP_OK) {
                return status;
            }
            if (opt->size_dopt.type == DIST_NONE) {
                log_stderr("mcperf: invalid distribution type %d for item "
                           "sizes", opt->size_dopt.type);
                return MCP_ERROR;
            }
            break;

        case '?':
            switch (optopt) {
            case 'o':
                log_stderr("mcperf: option -%c requires a file name", optopt);
                break;

            case 's':
            case 'm':
            case 'P':
            case 'c':
                log_stderr("mcperf: option -%c requires a string", optopt);
                break;

            case 'v':
            case 'p':
            case 'l':
            case 'b':
            case 'B':
            case 'e':
            case 'n':
            case 'N':
                log_stderr("mcperf: option -%c requires a number", optopt);
                break;

            case 't':
                log_stderr("mcperf: option -%c requires a real number", optopt);
                break;

            case 'r':
            case 'R':
            case 'z':
                log_stderr("mcperf: option -%c requires a distribution", optopt);
                break;

            default:
                log_stderr("mcperf: invalid option -- '%c'", optopt);
                break;
            }
            return MCP_ERROR;

        default:
            log_stderr("mcperf: invalid option -- '%c'", optopt);
            return MCP_ERROR;
        }
    }

    return MCP_OK;
}

static rstatus_t
mcp_pre_run(struct context *ctx)
{
    rstatus_t status;
    struct opt *opt = &ctx->opt;

    /* initialize logger */
    status = log_init(opt->log_level, opt->log_filename);
    if (status != MCP_OK) {
        return status;
    }

    /* initialize buffer */
    memset(ctx->buf1m, '0', sizeof(ctx->buf1m));

    /* resolve server info */
    status = mcp_resolve_addr(opt->server, opt->port, &opt->si);
    if (status != MCP_OK) {
        return status;
    }

    /*
     * Initialize distribution for {conn, call, size} load generators with
     * either default or user-supplied values.
     */
    dist_init(&ctx->conn_dist, opt->conn_dopt.type, opt->conn_dopt.min,
              opt->conn_dopt.max, opt->client.id);

    dist_init(&ctx->call_dist, opt->call_dopt.type, opt->call_dopt.min,
              opt->call_dopt.max, opt->client.id);

    dist_init(&ctx->size_dist, opt->size_dopt.type, opt->size_dopt.min,
              opt->size_dopt.max, opt->client.id);

    /* initialize stats subsystem */
    stats_init(ctx);

    /* initialize timer */
    timer_init();

    /* initialize core */
    status = core_init(ctx);
    if (status != MCP_OK) {
        return status;
    }

    return MCP_OK;
}

static void
mcp_run(struct context *ctx)
{
    rstatus_t status;

    core_start(ctx);

    for (;;) {
        status = core_loop(ctx);
        if (status != MCP_OK) {
            break;
        }
    }
}

int
main(int argc, char **argv)
{
    rstatus_t status;
    static struct context ctx;

    mcp_set_default_options(&ctx);

    status = mcp_get_options(&ctx, argc, argv);
    if (status != MCP_OK) {
        mcp_show_usage();
        exit(1);
    }

    if (show_version) {
        log_stderr("This is mcperf-%s" CRLF, MCP_VERSION_STRING);
        if (show_help) {
            mcp_show_usage();
        }
        exit(0);
    }

    status = mcp_pre_run(&ctx);
    if (status != MCP_OK) {
        exit(1);
    }

    mcp_run(&ctx);

    return 0;
}
