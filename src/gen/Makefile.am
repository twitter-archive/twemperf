MAINTAINERCLEANFILES = Makefile.in

AM_CFLAGS = -Wall -Wshadow -Wconversion
AM_CFLAGS += -D_GNU_SOURCE -D_XOPEN_SOURCE
AM_CPPFLAGS = -I $(top_srcdir)/src

noinst_LIBRARIES = libgen.a

libgen_a_SOURCES =		\
	mcp_call_generator.c	\
	mcp_conn_generator.c	\
	mcp_size_generator.c
