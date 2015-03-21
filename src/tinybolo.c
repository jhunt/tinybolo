/*
  Copyright 2015 James Hunt <james@jameshunt.us>

  This file is part of Bolo.

  Bolo is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  Bolo is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along
  with Bolo.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <zmq.h>

static int debug      = 0;
static int interval   = 30;
static int foreground = 0;
static char *endpoint = "tcp://127.0.0.1:2999";
static char *config   = "/etc/tinybolo.conf";

#define COMMAND_MAX 8192
static char commands[COMMAND_MAX] = { 0 };

#define FALLBACK(string,fallback) (*(string) ? (string) : (fallback))

#define debugf(...) do { if (debug) fprintf(stderr, __VA_ARGS__); } while (0)

void bail(void)
{
	fprintf(stderr, "USAGE: tinybolo -i 30 -c /etc/tinybolo.conf -e tcp://10.0.0.1:2999\n");
	exit(1);
}

static int zsend(void *z, const char *s, int flags)
{
	int rc;

	if (!s) rc = zmq_send(z, "", 0,             flags);
	else    rc = zmq_send(z, s,  strlen(s) + 1, flags);

	if (rc < 0) {
		fprintf(stderr, "zmq_send failed: %s\n", zmq_strerror(errno));
		return 1;
	}
	return 0;
}

static int send_frames(void *z, int n, ...)
{
	if (zsend(z, NULL, ZMQ_SNDMORE) != 0) return 1;
	debugf("  >> [");

	va_list ap;
	va_start(ap, n);

	int i;
	for (i = n; i > 0; i--) {
		const char *s = va_arg(ap, const char *);
		if (zsend(z, s, i == 1 ? 0 : ZMQ_SNDMORE) != 0) return 1;
		debugf("%s%c", s, i == 1 ? ']' : '|');
	}
	debugf("\n");
	va_end(ap);

	return 0;
}

int main(int argc, char **argv)
{
	int i, rc;
	char buf[8192];
	FILE *io;
	pid_t pid;
	void *zmq = NULL, *z = NULL;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-i") == 0) {
			if (!argv[++i]) bail();
			interval = atoi(argv[i]);
			continue;
		}
		if (strcmp(argv[i], "-c") == 0) {
			if (!argv[++i]) bail();
			config = argv[i];
			continue;
		}
		if (strcmp(argv[i], "-e") == 0) {
			if (!argv[++i]) bail();
			endpoint = argv[i];
			continue;
		}
		if (strcmp(argv[i], "-F") == 0) {
			foreground = 1;
			continue;
		}
		if (strcmp(argv[i], "-D") == 0) {
			debug = 1;
			continue;
		}
		bail();
	}

	int null = open("/dev/null", O_RDWR);
	if (null < 0) {
		fprintf(stderr, "/dev/null: %s\n", strerror(errno));
		exit(1);
	}

	zmq = zmq_ctx_new();
	if (!zmq) {
		fprintf(stderr, "failed to create a 0MQ context: %s\n", zmq_strerror(errno));
		exit(2);
	}

	z = zmq_socket(zmq, ZMQ_PUSH);
	if (!z) {
		fprintf(stderr, "failed to create a 0MQ socket: %s\n", zmq_strerror(errno));
		exit(2);
	}

	rc = zmq_connect(z, endpoint);
	if (rc != 0) {
		fprintf(stderr, "failed to connect to '%s': %s\n", endpoint, zmq_strerror(errno));
		exit(2);
	}

	io = fopen(config, "r");
	if (!io) {
		fprintf(stderr, "failed to read %s: %s\n", config, strerror(errno));
		exit(2);
	}

	if (!foreground) {
		rc = chdir("/");
		if (rc != 0) {
			fprintf(stderr, "failed to chdir to /: %s\n", strerror(errno));
			exit(2);
		}

		pid = fork();
		if (pid < 0) {
			fprintf(stderr, "failed to fork: %s\n", strerror(errno));
			exit(2);
		}
		if (pid != 0)
			exit(0);

		pid = setsid();
		if (pid < 0) {
			fprintf(stderr, "failed to set session id: %s\n", strerror(errno));
			exit(2);
		}
	}

	int off = 0, n = 0;
	char *a, *b;
	while (fgets(buf, 8192, io) != NULL) {
		for (a = buf; *a && isspace(*a); a++);
		if (!*a || *a == '#') continue;

		for (b = a; *b && *b != '\n'; b++);
		*b = '\0';

		debugf("read command `%s'\n", a);

		n = strlen(a) + 1;
		if (off + n > COMMAND_MAX - 1) {
			fprintf(stderr, "too many collectors defined!  truncating...\n");
			break;
		}
		memcpy(commands + off, a, n);
		off += n;
	}
	fclose(io);

	debugf("starting main loop\n");
	char *cmd;
	for (;;) {
		cmd = commands;
		while (*cmd) {
			int pfd[2];
			rc = pipe(pfd);
			if (rc != 0) {
				debugf("pipe failed: %s\n", strerror(errno));
				continue;
			}

			pid = fork();
			if (pid < 0) {
				debugf("fork failed: %s\n", strerror(errno));
				continue;
			}
			if (pid == 0) {
				close(pfd[0]);

				rc = dup2(null, 0);
				if (rc < 0)
					debugf("failed to redirect stdin < /dev/null: %s\n", strerror(errno));

				rc = dup2(pfd[1], 1);
				if (rc < 0)
					debugf("failed to redirect stdout > (pipe): %s\n", strerror(errno));

				if (!foreground) {
					rc = dup2(null, 2);
					if (rc < 0)
						debugf("failed to redirect stderr > /dev/null: %s\n", strerror(errno));
				}

				execl("/bin/sh", "sh", "-c", cmd, NULL);
				debugf("exec failed: %s\n", strerror(errno));
				exit(0);
			}

			debugf("child [%i] running `%s'\n", pid, cmd);
			close(pfd[1]);
			io = fdopen(pfd[0], "r");
			if (!io) {
				debugf("fdopen failed: %s\n", strerror(errno));
				close(pfd[0]);

			} else {
next:
				while (fgets(buf, 8192, io) != NULL) {

#define TOKENIZE() do { \
	for (a = b; *a &&  isspace(*a); a++); if (!*a) goto next; \
	for (b = a; *b && !isspace(*b); b++); if (!*b) goto next; \
	*b++ = '\0'; \
} while (0)
#define REMAINDER() do { \
	for (a = b; *a && isspace(*a); a++); \
	for (b = a; *b && *b != '\n'; b++); *b = '\0'; \
} while (0)
					b = buf;
					char *ts, *name, *val;

					TOKENIZE();
					if (strcmp(a, "STATE") == 0) {
						debugf("STATEs are not supported\n");
						TOKENIZE(); ts   = a;
						TOKENIZE(); name = a;
						TOKENIZE(); val  = a;
						REMAINDER();
						send_frames(z, 5, "STATE", ts, name, val, a);

					} else if (strcmp(a, "COUNTER") == 0) {
						TOKENIZE(); ts  = a;
						TOKENIZE(); name = a;
						REMAINDER();
						send_frames(z, 4, "COUNTER", ts, name, FALLBACK(a, "1"));

					} else if (strcmp(a, "SAMPLE") == 0) {
						TOKENIZE(); ts   = a;
						TOKENIZE(); name = a;
						TOKENIZE(); val  = a;
						send_frames(z, 4, "SAMPLE", ts, name, val);

					} else if (strcmp(a, "RATE") == 0) {
						TOKENIZE(); ts   = a;
						TOKENIZE(); name = a;
						TOKENIZE(); val  = a;
						send_frames(z, 4, "RATE", ts, name, val);

					} else if (strcmp(a, "EVENT") == 0) {
						TOKENIZE(); ts   = a;
						TOKENIZE(); name = a;
						REMAINDER();
						send_frames(z, 4, "EVENT", ts, name, FALLBACK(a, ""));
					}
#undef TOKENIZE
#undef REMAINDER
				}
			}
			fclose(io);
			close(pfd[0]);

			if (waitpid(pid, &rc, 0) < 0)
				debugf("waitpid failed: %s\n", strerror(errno));
			if (rc != 0)
				debugf("`%s' exited %02x\n", cmd, rc);

			while (*cmd++);
		}

		debugf("sleeping for %i seconds\n", interval);
		sleep(interval);
	}

	rc = 0;
	zmq_setsockopt(z, ZMQ_LINGER, &rc, sizeof(rc));
	zmq_close(z);
	zmq_ctx_destroy(zmq);
	return 0;
}
