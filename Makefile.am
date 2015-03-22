# Makefile.am

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I build
EXTRA_DIST = README.md bootstrap

AM_CFLAGS = -Wall -g
LDADD = -lpthread -lzmq

sbin_PROGRAMS = tinybolo openwrt
tinybolo_SOURCES = src/tinybolo.c
openwrt_SOURCES  = src/openwrt.c
