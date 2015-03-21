# Makefile.am

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I build
EXTRA_DIST = README.md bootstrap

AM_CFLAGS = -Wall -g
LDADD = -lpthread -lzmq

sbin_PROGRAMS = tinybolo
tinybolo_SOURCES = src/tinybolo.c
