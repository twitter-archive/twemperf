MAINTAINERCLEANFILES = Makefile.in

AM_CFLAGS = -Wall -Wshadow -Wconversion
AM_CFLAGS += -D_GNU_SOURCE -D_XOPEN_SOURCE
AM_CPPFLAGS = -I $(top_srcdir)/src

noinst_LIBRARIES = libstats.a

libstats_a_SOURCES =	\
	mcp_call_stats.c	\
	mcp_conn_stats.c
