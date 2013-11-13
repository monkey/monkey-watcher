/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  WatchMonkey
 *  -----------
 *  Copyright (C) 2013, Eduardo Silva P. <edsiper@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>

/* ANSI Colors */
#define ANSI_BOLD "\033[1m"
#define ANSI_CYAN "\033[36m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_RED "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_GREEN "\033[32m"
#define ANSI_WHITE "\033[37m"
#define ANSI_RESET "\033[0m"

#define MSG(str, ...)  fprintf(stderr, str, ##__VA_ARGS__)

static void banner()
{
    fprintf(stdout, "Watch Monkey, a watchdog for Monkey and Duda I/O\n\n");
}

static void usage()
{
    fprintf(stdout, "Usage: watchmonkey MONKEY_BINARY_PATH\n\n");
    return;
}

static int wm_enable_coredumps()
{
    int rc;
    int fd;
    int val = 1;
    struct rlimit core_limits;

    core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
    rc = setrlimit(RLIMIT_CORE, &core_limits);

    if (rc != 0) {
        MSG("Warning: could not change RLIMIT_CORE (no core dumps will be generated\n");
        return -1;
    }

    /* Lets make sure the core filenames include the PID */
    fd = open("/proc/sys/kernel/core_uses_pid", O_WRONLY);
    if (fd == -1) {
        MSG("Warning: could not set /proc/sys/kernel/core_uses_pid to 1\n");
    }


    rc = write(fd, &val, sizeof(val));
    if (rc <= 0) {
        MSG("Warning: could not write to /proc/sys/kernel/core_uses_pid\n");
    }

    close(fd);

    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    int fd_out[2];
    int fd_log;
    pid_t cpid;
    char *bin;
    time_t cur_time;
    struct stat st;

    if (argc < 2) {
        usage();
        exit(EXIT_FAILURE);
    }

    /* some info */
    banner();

    /* enable core dumps files for this session */
    wm_enable_coredumps();

    bin = strdup(argv[1]);
    rc = stat(bin, &st);
    if (rc != 0) {
        MSG("Error: could not access %s\n", bin);
        exit(EXIT_FAILURE);
    }

    if (access(bin, X_OK) != 0) {
        MSG("Error: could not execute %s\n", bin);
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (pipe(fd_out) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        cur_time = time(NULL);

        printf("\n");
        printf("================= Starting Program =================\n");
        printf(">>> Date     : %s", ctime(&cur_time));
        fflush(stdout);

        cpid = fork();
        if (cpid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (cpid == 0) { /* child */
            close(fd_out[0]);

            dup2(fd_out[1], STDOUT_FILENO);

            char *cmd[] = {"monkey", NULL};
            execv(bin, cmd);
        } else { /* parent */
            int status;
            char ch;
            char logf[1024];

            close(fd_out[1]);

            printf(">>> Pid      : %i\n", cpid);
            snprintf(logf, 1024, "stdout.%i", cpid);
            fd_log = open(logf, O_CREAT | O_WRONLY, 0644);
            if (fd_log == -1) {
                MSG("Error, Could not open log file %s\n", logf);
            }
            else {
                printf(">>> Stdout   : %s\n", logf);
            }

            while (read(fd_out[0], &ch, 1) > 0) {
                //printf("%c", ch);
                if (fd_log) {
                    write(fd_log, &ch, sizeof(ch));
                }
            }

            if (waitpid(cpid, &status, 0) < 0) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }

            if (fd_log) {
                close(fd_log);
            }

            printf(">>> Exit code: %i\n", status);
            fflush(stdout);
        }
        fflush(stdout);
        sleep(1);

    }

    return 0;
}
