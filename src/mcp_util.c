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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <execinfo.h>
#include <errno.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <mcp_core.h>
#include <mcp_util.h>
#include <mcp_log.h>

static int
mcp_resolve_addr_inet(char *name, int port, struct sockinfo *si)
{
    int status;
    struct addrinfo *ai, *cai; /* head and current addrinfo */
    struct addrinfo hints;
    char *node, nodestr[MCP_INET_ADDRSTRLEN], service[MCP_UINTMAX_MAXLEN];
    bool found;

    ASSERT(mcp_valid_port(port));

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;     /* AF_INET or AF_INET6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;

    if (name != NULL) {
        uint32_t nodelen = MIN(MCP_INET_ADDRSTRLEN - 1, mcp_strlen(name));
        mcp_memcpy(nodestr, name, nodelen);
        nodestr[nodelen] = '\0';
        node = nodestr;
    } else {
        /*
         * If AI_PASSIVE flag is specified in hints.ai_flags, and node is
         * NULL, then the returned socket addresses will be suitable for
         * bind(2)ing a socket that will accept(2) connections. The returned
         * socket address will contain the wildcard IP address.
         */
        node = NULL;
        hints.ai_flags |= AI_PASSIVE;
    }

    mcp_snprintf(service, MCP_UINTMAX_MAXLEN, "%d", port);

    status = getaddrinfo(node, service, &hints, &ai);
    if (status < 0) {
        log_error("address resolution of node '%s' service '%s' failed: %s",
                  node, service, gai_strerror(status));
        return -1;
    }

    /*
     * getaddrinfo() can return a linked list of more than one addrinfo,
     * since we requested for both AF_INET and AF_INET6 addresses and the
     * host itself can be multi-homed. Since we don't care whether we are
     * using ipv4 or ipv6, we just use the first address from this collection
     * in the order in which it was returned.
     *
     * The sorting function used within getaddrinfo() is defined in RFC 3484;
     * the order can be tweaked for a particular system by editing
     * /etc/gai.conf
     */
    for (cai = ai, found = false; cai != NULL; cai = cai->ai_next) {
        si->family = cai->ai_family;
        si->addrlen = cai->ai_addrlen;
        mcp_memcpy(&si->addr, cai->ai_addr, si->addrlen);
        found = true;
        break;
    }

    freeaddrinfo(ai);

    return !found ? -1 : 0;
}

static int
mcp_resolve_addr_unix(char *name, struct sockinfo *si)
{
    struct sockaddr_un *un;
    uint32_t namelen = mcp_strlen(name);

    if (namelen >= MCP_UNIX_ADDRSTRLEN) {
        return -1;
    }

    un = &si->addr.un;

    un->sun_family = AF_UNIX;
    mcp_memcpy(un->sun_path, name, namelen);
    un->sun_path[namelen] = '\0';

    si->family = AF_UNIX;
    si->addrlen = sizeof(*un);
    /* si->addr is an alias of un */

    return 0;
}

/*
 * Resolve a hostname and service by translating it to socket address
 * and return it in si
 *
 * This routine is reentrant
 */
int
mcp_resolve_addr(char *name, int port, struct sockinfo *si)
{
    if (name != NULL && name[0] == '/') {
        return mcp_resolve_addr_unix(name, si);
    }

    return mcp_resolve_addr_inet(name, port, si);
}

int
mcp_set_nonblocking(int sd)
{
    int flags;

    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(sd, F_SETFL, flags | O_NONBLOCK);
}

int
mcp_set_tcpnodelay(int sd)
{
    int nodelay;
    socklen_t len;

    nodelay = 1;
    len = sizeof(nodelay);

    return setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
}

int
mcp_set_linger(int sd, int timeout)
{
    struct linger linger;
    socklen_t len;

    linger.l_onoff = 1;
    linger.l_linger = timeout;

    len = sizeof(linger);

    return setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, len);
}

int
mcp_set_sndbuf(int sd, int size)
{
    socklen_t len;

    len = sizeof(size);

    return setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, len);
}

int
mcp_set_rcvbuf(int sd, int size)
{
    socklen_t len;

    len = sizeof(size);

    return setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, len);
}

int
mcp_get_soerror(int sd)
{
    int status, err;
    socklen_t len;

    err = 0;
    len = sizeof(err);

    status = getsockopt(sd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (status == 0) {
        errno = err;
    }

    return status;
}

int
mcp_get_sndbuf(int sd)
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

int
mcp_get_rcvbuf(int sd)
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

bool
mcp_valid_port(int n)
{
    if (n < 1 || n > UINT16_MAX) {
        return false;
    }

    return true;
}

/*
 * Convert ascii representation of a positive integer to int. On error
 * return -1
 */
int
mcp_atoi(char *line)
{
    int errno_save, val;
    unsigned long int value;
    char *end;

    errno_save = errno;

    errno = 0;

    /*
     * unsigned long int strtoul(const char *nptr, char **endptr, int base);
     *
     * If endptr is not NULL, strtoul() stores the address of the first
     * invalid character in *endptr. If there were no digits at all, strtoul()
     * stores the original value of nptr in *endptr (and returns 0).
     *
     * In particular, if *nptr is not '\0' but **endptr is '\0' on return, the
     * entire string is valid.
     */

    value = strtoul(line, &end, 10);
    if (errno != 0 || end == line || *end != '\0') {
        val = -1;
    } else {
        val = (int)value;
    }

    errno = errno_save;

    return val;
}

/*
 * Convert ascii representation of a positive floating pont number to
 * a double. On error return -1.0
 */
double
mcp_atod(char *line)
{
    int errno_save;
    double value;
    char *end;

    errno_save = errno;

    errno = 0;

    /*
     * double strtod(const char *nptr, char **endptr);
     *
     * If endptr is not NULL, a pointer to the character after the last
     * character used in the conversion is stored in the location referenced
     * by endptr.
     *
     * If no conversion is performed, zero is returned and the value of
     * nptr is stored in the location referenced by endptr.
     *
     * If the correct value would cause overflow, plus or minus HUGE_VAL
     * (HUGE_VALF, HUGE_VALL) is returned (according to the sign of the
     * value), and ERANGE is stored in errno.  If the correct value would
     * cause underflow, zero is returned and ERANGE is stored in errno.
     */
    value = strtod(line, &end);
    if (errno != 0 || end == line || *end != '\0') {
        value = -1.0;
    }

    errno = errno_save;

    return value;
}

void *
_mcp_alloc(size_t size, char *name, int line)
{
    void *p;

    ASSERT(size != 0);

    p = malloc(size);
    if (p == NULL) {
        log_debug(LOG_ERR, "malloc(%zu) failed @ %s:%d", size, name, line);
    }

    return p;
}

void *
_mcp_zalloc(size_t size, char *name, int line)
{
    void *p;

    p = mcp_alloc(size);
    if (p != NULL) {
        memset(p, 0, size);
    }

    return p;
}

void *
_mcp_calloc(size_t nmemb, size_t size, char *name, int line)
{
    return _mcp_zalloc(nmemb * size, name, line);
}

void *
_mcp_realloc(void *ptr, size_t size, char *name, int line)
{
    void *p;

    ASSERT(size != 0);

    p = realloc(ptr, size);
    if (p == NULL) {
        log_debug(LOG_CRIT, "realloc(%zu) failed @ %s:%d", size, name, line);
    }

    return p;
}

void
_mcp_free(void *ptr)
{
    ASSERT(ptr != NULL);
    free(ptr);
}

void
mcp_stacktrace(int skip_count)
{
    void *stack[64];
    char **symbols;
    int size, i, j;

    size = backtrace(stack, 64);
    symbols = backtrace_symbols(stack, size);
    if (symbols == NULL) {
        return;
    }

    skip_count++; /* skip the current frame also */

    for (i = skip_count, j = 0; i < size; i++, j++) {
        loga("[%d] %s", j, symbols[i]);
    }

    free(symbols);
}

void
mcp_assert(const char *cond, const char *file, int line, int panic)
{
    log_error("assert '%s' failed @ (%s, %d)", cond, file, line);
    if (panic) {
        mcp_stacktrace(1);
        abort();
    }
}

int
_vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    int n;

    n = vsnprintf(buf, size, fmt, args);

    /*
     * The return value is the number of characters which would be written
     * into buf not including the trailing '\0'. If size is == 0 the
     * function returns 0.
     *
     * On error, the function also returns 0. This is to allow idiom such
     * as len += _vscnprintf(...)
     *
     * See: http://lwn.net/Articles/69419/
     */
    if (n <= 0) {
        return 0;
    }

    if (n < (int) size) {
        return n;
    }

    return (int)(size - 1);
}

int
_scnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = _vscnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}

/*
 * Send n bytes on a blocking descriptor
 */
ssize_t
_mcp_sendn(int sd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t	nsend;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        nsend = send(sd, ptr, nleft, 0);
        if (nsend < 0) {
            if (errno == EINTR) {
                continue;
            }
            return nsend;
        }
        if (nsend == 0) {
            return -1;
        }

        nleft -= (size_t)nsend;
        ptr += nsend;
    }

    return (ssize_t)n;
}

/*
 * Recv n bytes from a blocking descriptor
 */
ssize_t
_mcp_recvn(int sd, void *vptr, size_t n)
{
	size_t nleft;
	ssize_t	nrecv;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
        nrecv = recv(sd, ptr, nleft, 0);
        if (nrecv < 0) {
            if (errno == EINTR) {
                continue;
            }
            return nrecv;
        }
        if (nrecv == 0) {
            break;
        }

        nleft -= (size_t)nrecv;
        ptr += nrecv;
    }

    return (ssize_t)(n - nleft);
}

/*
 * Return the current time in microseconds since Epoch
 */
int64_t
mcp_usec_now(void)
{
    struct timeval now;
    int64_t usec;
    int status;

    status = gettimeofday(&now, NULL);
    if (status < 0) {
        log_error("gettimeofday failed: %s", strerror(errno));
        return -1;
    }

    usec = (int64_t)now.tv_sec * 1000000LL + (int64_t)now.tv_usec;

    return usec;
}
