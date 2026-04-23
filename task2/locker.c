#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>

volatile sig_atomic_t keep_running = 1;
int success_locks = 0;
char *stats_file = "stats.txt";

void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    char *filename = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "f:s:")) != -1) {
        switch (opt) {
            case 'f': filename = optarg; break;
            case 's': stats_file = optarg; break;
            default: exit(EXIT_FAILURE);
        }
    }

    if (!filename) {
        fprintf(stderr, "Usage: %s -f <file> [-s stats]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char lck_filename[256];
    snprintf(lck_filename, sizeof(lck_filename), "%s.lck", filename);

    signal(SIGINT, handle_sigint);

    while (keep_running) {
        int fd_lck;

        while ((fd_lck = open(lck_filename, O_CREAT | O_EXCL | O_WRONLY, 0644)) < 0) {
            if (errno != EEXIST) { perror("open"); exit(EXIT_FAILURE); }
            if (!keep_running) break;
            usleep(50000); 
        }

        if (!keep_running) break;

        char pid_str[16];
        int len = snprintf(pid_str, sizeof(pid_str), "%d", getpid());
        if (write(fd_lck, pid_str, len) != len) {
            perror("write");
            close(fd_lck);
            unlink(lck_filename);
            exit(EXIT_FAILURE);
        }
        close(fd_lck);

        sleep(1); 

        fd_lck = open(lck_filename, O_RDONLY);
        if (fd_lck >= 0) {
            char buf[16] = {0};
            if (read(fd_lck, buf, sizeof(buf) - 1) > 0) {
                if (atoi(buf) == getpid()) {
                    unlink(lck_filename);
                    success_locks++;
                } else {
                    fprintf(stderr, "Collision detected!\n");
                    close(fd_lck);
                    exit(EXIT_FAILURE);
                }
            }
            close(fd_lck);
        }
        
        usleep(100000); 
    }

    FILE *f_stats = fopen(stats_file, "a");
    if (f_stats) {
        fprintf(f_stats, "PID %d: %d success locks\n", getpid(), success_locks);
        fclose(f_stats);
    }
    return 0;
}
