MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -D_GNU_SOURCE -D_XOPEN_SOURCE
AM_CFLAGS = -Wall -Wshadow -Wconversion
AM_LDFLAGS = -lm -rdynamic

SUBDIRS = gen stats

bin_PROGRAMS = mcperf

mcperf_SOURCES =				\
	mcp_call.c mcp_call.h			\
	mcp_conn.c mcp_conn.h			\
	mcp_core.c mcp_core.h			\
	mcp_distribution.c mcp_distribution.h	\
	mcp_ecb.c mcp_ecb.h			\
	mcp_event.c mcp_event.h			\
	mcp_generator.c mcp_generator.h		\
	mcp_log.c mcp_log.h			\
	mcp_stats.c mcp_stats.h			\
	mcp_timer.c mcp_timer.h			\
	mcp_util.c mcp_util.h			\
	mcp_queue.h				\
	mcp.c

mcperf_LDADD = $(top_builddir)/src/gen/libgen.a
mcperf_LDADD += $(top_builddir)/src/stats/libstats.a
