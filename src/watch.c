
//
// watch.c
//
// Copyright (c) 2011 TJ Holowaychuk <tj@vision-media.ca>
//

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>

/*
 * Command version.
 */

#define VERSION "0.2.1"

/*
 * Default interval in milliseconds.
 */

#define DEFAULT_INTERVAL 1000

/*
 * Quiet mode.
 */

static int quiet = 0;

/*
 * Halt on failure.
 */

static int halt = 0;

/*
 * Command line options.
 */

static struct option longopts[] = {
  { "quiet",    no_argument,       NULL, 'q' },
  { "halt",     no_argument,       NULL, 'x' },
  { "interval", required_argument, NULL, 'i' },
  { "version",  no_argument,       NULL, 'v' },
  { "help",     no_argument,       NULL, 'h' },
};

/*
 * Output command usage.
 */

void
usage() {
  printf(
    "\n"
    "  Usage: watch [options] <cmd>\n"
    "\n"
    "  Options:\n"
    "\n"
    "    -q, --quiet           only output stderr\n"
    "    -x, --halt            halt on failure\n"
    "    -i, --interval <n>    interval in seconds or ms defaulting to 1\n"
    "    -v, --version         output version number\n"
    "    -h, --help            output this help information\n"
    "\n"
    );
  exit(1);
}

/*
 * Milliseconds string.
 */

int
milliseconds(const char *str) {
  int len = strlen(str);
  return 'm' == str[len-2] && 's' == str[len-1];
}

/*
 * Sleep in `ms`.
 */

void
mssleep(int ms) {
  struct timespec req = {0};
  time_t sec = (int)(ms / 1000);
  ms = ms -(sec * 1000);
  req.tv_sec = sec;
  req.tv_nsec = ms * 1000000L;
  while(-1 == nanosleep(&req, &req)) ;
}

/*
 * Redirect stdout to `path`.
 */

void
redirect_stdout(const char *path) {
  int fd = open(path, O_WRONLY);
  if (dup2(fd, 1) < 0) {
    perror("dup2()");
    exit(1); 
  }
}

/*
 * Return the total string-length consumed by `strs`.
 */

int
length(char **strs, int len) {
  int i = 0;
  int n = 0;
  for (i = 0; i < len; i++)
    n += strlen(strs[i]);
  return n + 1;
}

/*
 * Join the given `strs` with `val`.
 */

char *
join(char **strs, int len, char *val) {
  --len;
  char *buf = calloc(1, length(strs, len) + len * strlen(val) + 1);
  char *str;
  while ((str = *strs++)) {
    strcat(buf, str);
    if (*strs) strcat(buf, val);
  }
  return buf;
}

/*
 * Parse argv.
 */

int
main(int argc, char **argv) {
  if (1 == argc) usage();
  int interval = DEFAULT_INTERVAL;

  int ch;

  while ((ch = getopt_long(argc, argv, "hqxvi:", longopts, NULL)) != -1) {
    switch (ch) {

    // -h, --help
    case 'h':
      usage();

    // -q, --quiet
    case 'q':
      quiet = 1;
      break;

    // -x, --halt
    case 'x':
      halt = 1;
      break;

    // -v, --version
    case 'v':
      printf("%s\n", VERSION);
      exit(1);

    // -i, --interval <n>
    case 'i':
      interval = milliseconds(optarg)
        ? atoi(optarg)
        : atoi(optarg) * 1000;
      break;

    // unknown option
    default:
      usage();
    }
  }

  // cmd args
  argc -= optind;
  argv += optind;

  // <cmd>
  if (!argc) {
    fprintf(stderr, "\n  <cmd> required\n\n");
    exit(1);
  }

  // cmd
  char *val = join(argv, argc, " ");
  char *cmd[4] = { "sh", "-c", val, 0 };

  // exec loop
  loop: {
    pid_t pid;
    int status;
    switch (pid = fork()) {
      // error
      case -1:
        perror("fork()");
        exit(1);
      // child
      case 0:
        if (quiet) redirect_stdout("/dev/null");
        execvp(cmd[0], cmd);
      // parent
      default:
        if (waitpid(pid, &status, 0) < 0) {
          perror("waitpid()");
          exit(1);
        }

        // exit > 0
        if (WEXITSTATUS(status)) {
          fprintf(stderr, "\033[90mexit: %d\33[0m\n\n", WEXITSTATUS(status));
          if (halt) exit(WEXITSTATUS(status));
        }

        mssleep(interval);
        goto loop;
    }    
  }

  return 0;
}
