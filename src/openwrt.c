/*
  Copyright 2015 James Hunt <james@jameshunt.us>

  This file is part of tinybolo.

  tinybolo is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  tinybolo is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along
  with tinybolo.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#define PROC "/proc"

static const char *PREFIX;
static int32_t ts;
static char buf[8192];

static int32_t time_s(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec;
}

#define streq(a,b) (strcmp((a), (b)) == 0)

int collect_meminfo(void);
int collect_loadavg(void);
int collect_stat(void);
int collect_procs(void);
int collect_openfiles(void);
int collect_mounts(void);
int collect_vmstat(void);
int collect_diskstats(void);
int collect_netdev(void);

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "USAGE: %s prefix\n", argv[0]);
		exit(1);
	}

	PREFIX = argv[1];
	int rc = 0;

	rc += collect_meminfo();
	rc += collect_loadavg();
	rc += collect_stat();
	rc += collect_procs();
	rc += collect_openfiles();
	rc += collect_mounts();
	rc += collect_vmstat();
	rc += collect_diskstats();
	rc += collect_netdev();
	return rc;
}

int collect_meminfo(void)
{
	FILE *io = fopen(PROC "/meminfo", "r");
	if (!io)
		return 1;

	struct {
		uint32_t total;
		uint32_t used;
		uint32_t free;
		uint32_t buffers;
		uint32_t cached;
		uint32_t slab;
	} M = { 0 };
	struct {
		uint32_t total;
		uint32_t used;
		uint32_t free;
		uint32_t cached;
	} S = { 0 };
	uint32_t x;
	ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		/* MemTotal:        6012404 kB\n */
		char *k, *v, *u, *e;

		k = buf; v = strchr(k, ':');
		if (!v || !*v) continue;

		*v++ = '\0';
		while (isspace(*v)) v++;
		u = strchr(v, ' ');
		if (u) {
			*u++ = '\0';
		} else {
			u = strchr(v, '\n');
			if (u) *u = '\0';
			u = NULL;
		}

		x = strtoul(v, &e, 10);
		if (*e) continue;

		if (u && *u == 'k')
			x *= 1024;

		     if (streq(k, "MemTotal"))   M.total   = x;
		else if (streq(k, "MemFree"))    M.free    = x;
		else if (streq(k, "Buffers"))    M.buffers = x;
		else if (streq(k, "Cached"))     M.cached  = x;
		else if (streq(k, "Slab"))       M.slab    = x;

		else if (streq(k, "SwapTotal"))  S.total   = x;
		else if (streq(k, "SwapFree"))   S.free    = x;
		else if (streq(k, "SwapCached")) S.cached  = x;
	}

	M.used = M.total - (M.free + M.buffers + M.cached + M.slab);
	printf("SAMPLE %i %s:memory:total %u\n",   ts, PREFIX, M.total);
	printf("SAMPLE %i %s:memory:used %u\n",    ts, PREFIX, M.used);
	printf("SAMPLE %i %s:memory:free %u\n",    ts, PREFIX, M.free);
	printf("SAMPLE %i %s:memory:buffers %u\n", ts, PREFIX, M.buffers);
	printf("SAMPLE %i %s:memory:cached %u\n",  ts, PREFIX, M.cached);
	printf("SAMPLE %i %s:memory:slab %u\n",    ts, PREFIX, M.slab);

	S.used = S.total - (S.free + S.cached);
	printf("SAMPLE %i %s:swap:total %u\n",   ts, PREFIX, S.total);
	printf("SAMPLE %i %s:swap:cached %u\n",  ts, PREFIX, S.cached);
	printf("SAMPLE %i %s:swap:used %u\n",    ts, PREFIX, S.used);
	printf("SAMPLE %i %s:swap:free %u\n",    ts, PREFIX, S.free);

	fclose(io);
	return 0;
}

int collect_loadavg(void)
{
	FILE *io = fopen(PROC "/loadavg", "r");
	if (!io)
		return 1;

	double load[3];
	uint64_t proc[3];

	ts = time_s();
	int rc = fscanf(io, "%lf %lf %lf %lu/%lu ",
			&load[0], &load[1], &load[2], &proc[0], &proc[1]);
	fclose(io);
	if (rc < 5)
		return 1;

	if (proc[0])
		proc[0]--; /* don't count us */

	printf("SAMPLE %i %s:load:1min"        " %0.2f\n",  ts, PREFIX, load[0]);
	printf("SAMPLE %i %s:load:5min"        " %0.2f\n",  ts, PREFIX, load[1]);
	printf("SAMPLE %i %s:load:15min"       " %0.2f\n",  ts, PREFIX, load[2]);
	printf("SAMPLE %i %s:load:runnable"    " %lu\n",    ts, PREFIX, proc[0]);
	printf("SAMPLE %i %s:load:schedulable" " %lu\n",    ts, PREFIX, proc[1]);
	return 0;
}

int collect_stat(void)
{
	FILE *io = fopen(PROC "/stat", "r");
	if (!io)
		return 1;

	int cpus = 0;
	ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		char *k, *v, *p;

		k = v = buf;
		while (*v && !isspace(*v)) v++; *v++ = '\0';
		p = strchr(v, '\n'); if (p) *p = '\0';

		if (streq(k, "processes"))
			printf("RATE %i %s:ctxt:forks-s %s\n", ts, PREFIX, v);
		else if (streq(k, "ctxt"))
			printf("RATE %i %s:ctxt:cswch-s %s\n", ts, PREFIX, v);
		else if (strncmp(k, "cpu", 3) == 0 && isdigit(k[3]))
			cpus++;

		if (streq(k, "cpu")) {
			while (*v && isspace(*v)) v++;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:user %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:nice %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:system %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:idle %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:iowait %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:irq %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:softirq %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:steal %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:guest %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:guest-nice %s\n", ts, PREFIX, v); v = k;
		}
	}
	printf("SAMPLE %i %s:load:cpus %i\n", ts, PREFIX, cpus);

	fclose(io);
	return 0;
}

int collect_procs(void)
{
	struct {
		uint16_t running;
		uint16_t sleeping;
		uint16_t zombies;
		uint16_t stopped;
		uint16_t paging;
		uint16_t blocked;
		uint16_t unknown;
	} P = {0};

	int pid;
	struct dirent *dir;
	DIR *d = opendir(PROC);
	if (!d)
		return 1;

	ts = time_s();
	while ((dir = readdir(d)) != NULL) {
		if (!isdigit(dir->d_name[0])
		 || (pid = atoi(dir->d_name)) < 1)
			continue;

		char *file;
		if (asprintf(&file, PROC "/%i/stat", pid) != 0)
			continue;

		FILE *io = fopen(file, "r");
		free(file);
		if (!io)
			continue;

		char *a;
		if (!fgets(buf, 8192, io)) {
			fclose(io);
			continue;
		}
		fclose(io);

		a = buf;
		/* skip PID */
		while (*a && !isspace(*a)) a++;
		while (*a &&  isspace(*a)) a++;
		/* skip progname */
		while (*a && !isspace(*a)) a++;
		while (*a &&  isspace(*a)) a++;

		switch (*a) {
		case 'R': P.running++;  break;
		case 'S': P.sleeping++; break;
		case 'D': P.blocked++;  break;
		case 'Z': P.zombies++;  break;
		case 'T': P.stopped++;  break;
		case 'W': P.paging++;   break;
		default:  P.unknown++;  break;
		}
	}

	printf("SAMPLE %i %s:procs:running %i\n",  ts, PREFIX, P.running);
	printf("SAMPLE %i %s:procs:sleeping %i\n", ts, PREFIX, P.sleeping);
	printf("SAMPLE %i %s:procs:blocked %i\n",  ts, PREFIX, P.blocked);
	printf("SAMPLE %i %s:procs:zombies %i\n",  ts, PREFIX, P.zombies);
	printf("SAMPLE %i %s:procs:stopped %i\n",  ts, PREFIX, P.stopped);
	printf("SAMPLE %i %s:procs:paging %i\n",   ts, PREFIX, P.paging);
	printf("SAMPLE %i %s:procs:unknown %i\n",  ts, PREFIX, P.unknown);
	return 0;
}

int collect_openfiles(void)
{
	FILE *io = fopen(PROC "/sys/fs/file-nr", "r");
	if (!io)
		return 1;

	ts = time_s();
	char *a, *b;
	if (!fgets(buf, 8192, io)) {
		fclose(io);
		return 1;
	}

	a = buf;
	/* used file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:used %s\n", ts, PREFIX, a);

	a = b;
	/* free file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:free %s\n", ts, PREFIX, a);

	a = b;
	/* max file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:max %s\n", ts, PREFIX, a);

	return 0;
}

int collect_mounts(void)
{
	FILE *io = fopen(PROC "/mounts", "r");
	if (!io)
		return 1;

	struct stat st;
	struct statvfs fs;
	char *a, *b, *c;
	ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		a = b = buf;
		for (b = buf; *b && !isspace(*b); b++); *b++ = '\0';
		for (c = b;   *c && !isspace(*c); c++); *c++ = '\0';
		char *dev = a, *path = b;

		if (lstat(path, &st) != 0
		 || statvfs(path, &fs) != 0
		 || !major(st.st_dev))
			continue;

		printf("KEY %s:fs:%s %s\n",  PREFIX, path, dev);
		printf("KEY %s:dev:%s %s\n", PREFIX, dev, path);

		printf("SAMPLE %i %s:df:%s:inodes.total %lu\n", ts, PREFIX, path, fs.f_files);
		printf("SAMPLE %i %s:df:%s:inodes.free %lu\n",  ts, PREFIX, path, fs.f_favail);
		printf("SAMPLE %i %s:df:%s:inodes.rfree %lu\n", ts, PREFIX, path, fs.f_ffree - fs.f_favail);

		printf("SAMPLE %i %s:df:%s:bytes.total %lu\n", ts, PREFIX, path, fs.f_frsize *  fs.f_blocks);
		printf("SAMPLE %i %s:df:%s:bytes.free %lu\n",  ts, PREFIX, path, fs.f_frsize *  fs.f_bavail);
		printf("SAMPLE %i %s:df:%s:bytes.rfree %lu\n", ts, PREFIX, path, fs.f_frsize * (fs.f_bfree - fs.f_bavail));
	}

	fclose(io);
	return 0;
}

int collect_vmstat(void)
{
	FILE *io = fopen(PROC "/vmstat", "r");
	if (!io)
		return 1;

	uint64_t pgsteal = 0;
	uint64_t pgscan_kswapd = 0;
	uint64_t pgscan_direct = 0;
	ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		char name[64];
		uint64_t value;
		int rc = sscanf(buf, "%63s %lu\n", name, &value);
		if (rc < 2)
			continue;

#define VMSTAT_SIMPLE(x,n,v,t) do { \
	if (streq((n), #t)) printf("RATE %i %s:vm:%s %lu\n", ts, PREFIX, #t, (v)); \
} while (0)
		VMSTAT_SIMPLE(VM, name, value, pswpin);
		VMSTAT_SIMPLE(VM, name, value, pswpout);
		VMSTAT_SIMPLE(VM, name, value, pgpgin);
		VMSTAT_SIMPLE(VM, name, value, pgpgout);
		VMSTAT_SIMPLE(VM, name, value, pgfault);
		VMSTAT_SIMPLE(VM, name, value, pgmajfault);
		VMSTAT_SIMPLE(VM, name, value, pgfree);
#undef  VMSTAT_SIMPLE

		if (strncmp(name, "pgsteal_", 8) == 0)        pgsteal       += value;
		if (strncmp(name, "pgscan_kswapd_", 14) == 0) pgscan_kswapd += value;
		if (strncmp(name, "pgscan_direct_", 14) == 0) pgscan_direct += value;
	}
	printf("RATE %i %s:vm:pgsteal %lu\n",       ts, PREFIX, pgsteal);
	printf("RATE %i %s:vm:pgscan.kswapd %lu\n", ts, PREFIX, pgscan_kswapd);
	printf("RATE %i %s:vm:pgscan.direct %lu\n", ts, PREFIX, pgscan_direct);

	fclose(io);
	return 0;
}

/* FIXME: figure out a better way to detect devices */
#define is_device(dev) (strncmp((dev), "loop", 4) != 0 && strncmp((dev), "ram", 3) != 0)

int collect_diskstats(void)
{
	FILE *io = fopen(PROC "/diskstats", "r");
	if (!io)
		return 1;

	uint32_t dev[2];
	uint64_t rd[4], wr[4];
	ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		char name[32];
		int rc = sscanf(buf, "%u %u %31s %lu %lu %lu %lu %lu %lu %lu %lu",
				&dev[0], &dev[1], name,
				&rd[0], &rd[1], &rd[2], &rd[3],
				&wr[0], &wr[1], &wr[2], &wr[3]);
		if (rc != 11)
			continue;
		if (!is_device(name))
			continue;

		printf("RATE %i %s:diskio:%s:rd-iops %lu\n",  ts, PREFIX, name, rd[0]);
		printf("RATE %i %s:diskio:%s:rd-miops %lu\n", ts, PREFIX, name, rd[1]);
		printf("RATE %i %s:diskio:%s:rd-msec %lu\n",  ts, PREFIX, name, rd[2]);
		printf("RATE %i %s:diskio:%s:rd-bytes %lu\n", ts, PREFIX, name, rd[3 ] * 512);

		printf("RATE %i %s:diskio:%s:wr-iops %lu\n",  ts, PREFIX, name, wr[0]);
		printf("RATE %i %s:diskio:%s:wr-miops %lu\n", ts, PREFIX, name, wr[1]);
		printf("RATE %i %s:diskio:%s:wr-msec %lu\n",  ts, PREFIX, name, wr[2]);
		printf("RATE %i %s:diskio:%s:wr-bytes %lu\n", ts, PREFIX, name, wr[3] * 512);
	}

	fclose(io);
	return 0;
}

int collect_netdev(void)
{
	FILE *io = fopen(PROC "/net/dev", "r");
	if (!io)
		return 1;

	ts = time_s();
	if (fgets(buf, 8192, io) == NULL
	 || fgets(buf, 8192, io) == NULL) {
		fclose(io);
		return 1;
	}

	struct {
		uint64_t bytes;
		uint64_t packets;
		uint64_t errors;
		uint64_t drops;
		uint64_t overruns;
		uint64_t frames;
		uint64_t compressed;
		uint64_t collisions;
		uint64_t multicast;
		uint64_t carrier;
	} tx = {0xff}, rx = {0xff};

	while (fgets(buf, 8192, io) != NULL) {
		char name[32];
		int rc = sscanf(buf, " %31s "
			"%lu %lu %lu %lu %lu %lu %lu %lu "
			"%lu %lu %lu %lu %lu %lu %lu %lu\n",
			name,
			&rx.bytes, &rx.packets, &rx.errors, &rx.drops,
			&rx.overruns, &rx.frames, &rx.compressed, &rx.multicast,
			&tx.bytes, &tx.packets, &tx.errors, &tx.drops,
			&tx.overruns, &tx.collisions, &tx.carrier, &tx.compressed);

		if (rc < 17)
			continue;

		char *x = strrchr(name, ':');
		if (x) *x = '\0';

		printf("RATE %i %s:net:%s:rx.bytes %lu\n",      ts, PREFIX, name, rx.bytes);
		printf("RATE %i %s:net:%s:rx.packets %lu\n",    ts, PREFIX, name, rx.packets);
		printf("RATE %i %s:net:%s:rx.errors %lu\n",     ts, PREFIX, name, rx.errors);
		printf("RATE %i %s:net:%s:rx.drops %lu\n",      ts, PREFIX, name, rx.drops);
		printf("RATE %i %s:net:%s:rx.overruns %lu\n",   ts, PREFIX, name, rx.overruns);
		printf("RATE %i %s:net:%s:rx.compressed %lu\n", ts, PREFIX, name, rx.compressed);
		printf("RATE %i %s:net:%s:rx.frames %lu\n",     ts, PREFIX, name, rx.frames);
		printf("RATE %i %s:net:%s:rx.multicast %lu\n",  ts, PREFIX, name, rx.multicast);

		printf("RATE %i %s:net:%s:tx.bytes %lu\n",      ts, PREFIX, name, tx.bytes);
		printf("RATE %i %s:net:%s:tx.packets %lu\n",    ts, PREFIX, name, tx.packets);
		printf("RATE %i %s:net:%s:tx.errors %lu\n",     ts, PREFIX, name, tx.errors);
		printf("RATE %i %s:net:%s:tx.drops %lu\n",      ts, PREFIX, name, tx.drops);
		printf("RATE %i %s:net:%s:tx.overruns %lu\n",   ts, PREFIX, name, tx.overruns);
		printf("RATE %i %s:net:%s:tx.compressed %lu\n", ts, PREFIX, name, tx.compressed);
		printf("RATE %i %s:net:%s:tx.collisions %lu\n", ts, PREFIX, name, tx.collisions);
		printf("RATE %i %s:net:%s:tx.carrier %lu\n",    ts, PREFIX, name, tx.carrier);
	}

	fclose(io);
	return 0;
}
