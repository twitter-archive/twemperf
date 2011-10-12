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

#ifndef _MCP_UTIL_H_
#define _MCP_UTIL_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#define SQUARE(d)           ((d) * (d))
#define VAR(s, s2, n)       \
    (((n) < 2) ? 0.0 : ((s2) - SQUARE(s) / (n)) / ((n) - 1))
#define STDDEV(s, s2, n)    (((n) < 2) ? 0.0 : sqrt(VAR((s), (s2), (n))))

#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) > (b) ? (a) : (b))

#define NELEM(a)    ((sizeof(a)) / sizeof((a)[0]))

#define KB  (1024)
#define MB  (1024 * KB)
#define GB  (1024 * MB)

/*
 * Length of 1 byte, 2 bytes, 4 bytes, 8 bytes and largest integral
 * type (uintmax_t) in ascii, including the null terminator '\0'
 *
 * From stdint.h, we have:
 * # define UINT8_MAX	(255)
 * # define UINT16_MAX	(65535)
 * # define UINT32_MAX	(4294967295U)
 * # define UINT64_MAX	(__UINT64_C(18446744073709551615))
 */
#define MCP_UINT8_MAXLEN    (3 + 1)
#define MCP_UINT16_MAXLEN   (5 + 1)
#define MCP_UINT32_MAXLEN   (10 + 1)
#define MCP_UINT64_MAXLEN   (20 + 1)
#define MCP_UINTMAX_MAXLEN  MCP_UINT64_MAXLEN

/* timeval to seconds */
#define TV_TO_SEC(_tv)  ((_tv)->tv_sec + (1e-6 * (_tv)->tv_usec))

#define MCP_INET4_ADDRSTRLEN    (sizeof("255.255.255.255") - 1)
#define MCP_INET6_ADDRSTRLEN    \
    (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)
#define MCP_INET_ADDRSTRLEN     MAX(MCP_INET4_ADDRSTRLEN, MCP_INET6_ADDRSTRLEN)
#define MCP_UNIX_ADDRSTRLEN     \
    (sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

/*
 * Unified socket address combining inet and unix
 * domain sockets
 */
struct sockinfo {
    int       family;              /* socket address family */
    socklen_t addrlen;             /* socket address length */
    union {
        struct sockaddr_in  in;    /* ipv4 socket address */
        struct sockaddr_in6 in6;   /* ipv6 socket address */
        struct sockaddr_un  un;    /* unix domain address */
    } addr;
};

int mcp_resolve_addr(char *name, int port, struct sockinfo *si);

int mcp_set_nonblocking(int sd);
int mcp_set_tcpnodelay(int sd);
int mcp_set_linger(int sd, int timeout);
int mcp_set_sndbuf(int sd, int size);
int mcp_set_rcvbuf(int sd, int size);

int mcp_get_soerror(int sd);
int mcp_get_sndbuf(int sd);
int mcp_get_rcvbuf(int sd);

bool mcp_valid_port(int n);
int mcp_atoi(char *line);
double mcp_atod(char *line);

/*
 * Wrapper around common routines for manipulating C character
 * strings
 */
#define mcp_memcpy(_d, _c, _n)          \
    memcpy(_d, _c, (size_t)(_n))

#define mcp_memmove(_d, _c, _n)         \
    memmove(_d, _c, (size_t)(_n))

#define mcp_memchr(_d, _c, _n)          \
    memchr(_d, _c, (size_t)(_n))

#define mcp_strlen(_s)                   \
    strlen((char *)(_s))

#define mcp_strncmp(_s1, _s2, _n)        \
    strncmp((char *)(_s1), (char *)(_s2), (size_t)(_n))

#define mcp_strchr(_p, _l, _c)           \
    _mcp_strchr((uint8_t *)(_p), (uint8_t *)(_l), (uint8_t)(_c))

#define mcp_strrchr(_p, _s, _c)          \
    _mcp_strrchr((uint8_t *)(_p),(uint8_t *)(_s), (uint8_t)(_c))

#define mcp_strndup(_s, _n)              \
    (uint8_t *)strndup((char *)(_s), (size_t)(_n));

#define mcp_snprintf(_s, _n, ...)        \
    snprintf((char *)(_s), (size_t)(_n), __VA_ARGS__)

#define mcp_scnprintf(_s, _n, ...)       \
    _scnprintf((char *)(_s), (size_t)(_n), __VA_ARGS__)

#define mcp_vscnprintf(_s, _n, _f, _a)   \
    _vscnprintf((char *)(_s), (size_t)(_n), _f, _a)

static inline uint8_t *
_mcp_strchr(uint8_t *p, uint8_t *last, uint8_t c)
{
    while (p < last) {
        if (*p == c) {
            return p;
        }
        p++;
    }

    return NULL;
}

static inline uint8_t *
_mcp_strrchr(uint8_t *p, uint8_t *start, uint8_t c)
{
    while (p >= start) {
        if (*p == c) {
            return p;
        }
        p--;
    }

    return NULL;
}

/*
 * Memory allocation and free wrappers.
 *
 * These wrappers enables us to loosely detect double free, dangling
 * pointer access and zero-byte alloc.
 */
#define mcp_alloc(_s)        _mcp_alloc((size_t)(_s), __FILE__, __LINE__)
#define mcp_zalloc(_s)       _mcp_zalloc((size_t)(_s), __FILE__, __LINE__)
#define mcp_calloc(_n, _s)   _mcp_calloc((size_t)(_n), (size_t)(_s), __FILE__, __LINE__)
#define mcp_realloc(_p, _s)  _mcp_realloc(_p, (size_t)(_s), __FILE__, __LINE__)
#define mcp_free(_p) do {   \
    _mcp_free(_p);          \
    (_p) = NULL;            \
} while (0)

void *_mcp_alloc(size_t size, char *name, int line);
void *_mcp_zalloc(size_t size, char *name, int line);
void *_mcp_calloc(size_t nmemb, size_t size, char *name, int line);
void *_mcp_realloc(void *ptr, size_t size, char *name, int line);
void _mcp_free(void *ptr);

/*
 * Wrappers to send or receive n byte message on a blocking
 * socket descriptor.
 */
#define mcp_sendn(_s, _b, _n)    _mcp_sendn(_s, _b, (size_t)(_n))
#define mcp_recvn(_s, _b, _n)    _mcp_recvn(_s, _b, (size_t)(_n))

/*
 * Wrappers to read or write data to/from (multiple) buffers
 * to a file or socket descriptor.
 */
#define mcp_read(_d, _b, _n)     read(_d, _b, (size_t)(_n))
#define mcp_readv(_d, _b, _n)    readv(_d, _b, (int)(_n))

#define mcp_write(_d, _b, _n)    write(_d, _b, (size_t)(_n))
#define mcp_writev(_d, _b, _n)   writev(_d, _b, (int)(_n))

ssize_t _mcp_sendn(int sd, const void *vptr, size_t n);
ssize_t _mcp_recvn(int sd, void *vptr, size_t n);

void mcp_assert(const char *cond, const char *file, int line, int panic);

#ifdef MCP_ASSERT_PANIC

#define ASSERT(_x) do {                         \
    if (!(_x)) {                                \
        mcp_assert(#_x, __FILE__, __LINE__, 1); \
    }                                           \
} while (0)

#define NOT_REACHED() ASSERT(0)

#elif MCP_ASSERT_LOG

#define ASSERT(_x) do {                         \
    if (!(_x)) {                                \
        mcp_assert(#_x, __FILE__, __LINE__, 0); \
    }                                           \
} while (0)

#define NOT_REACHED() ASSERT(0)

#else

#define ASSERT(_x)

#define NOT_REACHED()

#endif

void mcp_stacktrace(int skip_count);
int _scnprintf(char *buf, size_t size, const char *fmt, ...);
int _vscnprintf(char *buf, size_t size, const char *fmt, va_list args);

int64_t mcp_usec_now(void);

#endif
