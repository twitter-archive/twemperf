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

#ifndef _MCP_DISTRIBUTION_H_
#define _MCP_DISTRIBUTION_H_

struct dist_info;

typedef void (*dist_next_t)(struct dist_info *);

typedef enum dist_type {
    DIST_NONE,          /* invalid or special case */
    DIST_DETERMINISTIC, /* fixed or deterministic */
    DIST_UNIFORM,       /* uniform over interval [min, max) */
    DIST_EXPONENTIAL,   /* poisson with mean */
    DIST_SEQUENTIAL,    /* sequential or monotonic */
    DIST_SENTINEL
} dist_type_t;

struct dist_opt {
    dist_type_t type;      /* distribution type */
    double      min;       /* minimum value */
    double      max;       /* maximum value */
};

struct dist_info {
    dist_type_t type;      /* distribution type */

    uint16_t    xsubi[3];  /* erand48 seed */
    double      min;       /* minimum value */
    double      max;       /* maximum value */

    dist_next_t next;      /* next handler */
    uint32_t    next_id;   /* next distribution value id */
    double      next_val;  /* next distribution value */
};

void dist_init(struct dist_info *di, dist_type_t type, double min, double max, uint32_t id);

#endif
