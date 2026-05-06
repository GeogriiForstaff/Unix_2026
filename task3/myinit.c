#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <time.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h> 

#define MAX_CHILDREN 100
#define LOG_PATH "/tmp/myinit.log"

typedef struct {
    char path[256];
    char arg[256];
    char in[256];
    char out[256];
    pid_t pid;
} ChildConfig;

ChildConfig children[MAX_CHILDREN];
int child_count = 0;
char config_path[PATH_MAX]; 
volatile sig_atomic_t reload_config = 0;

void log_message(const char *msg) {
    int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        time_t now = time(NULL);
        char *t_str = ctime(&now);
        if (t_str) t_str[strlen(t_str) - 1] = '\0';
        char buf[1024];
        int len = snprintf(buf, sizeof(buf), "[%s] %s\n", t_str ? t_str : "unknown", msg);
        if (write(fd, buf, len) < 0) { /* ignore */ }
        close(fd);
    }
}

void start_child(int idx) {
    pid_t pid = fork();
    if (pid == -1) {
        log_message("Critical: Fork failed!");
        return;
    }
    if (pid == 0) {
        int fd_in = open(children[idx].in, O_RDONLY);
        if (fd_in >= 0) { dup2(fd_in, STDIN_FILENO); close(fd_in); }

        int fd_out = open(children[idx].out, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd_out >= 0) { dup2(fd_out, STDOUT_FILENO); close(fd_out); }

        char *args[] = {children[idx].path, children[idx].arg, NULL};
        execv(children[idx].path, args);
        exit(EXIT_FAILURE);
    } else {
        children[idx].pid = pid;
        char msg[512];
        snprintf(msg, sizeof(msg), "Started: %s (PID %d)", children[idx].path, pid);
        log_message(msg);
    }
}

void read_config() {
    FILE *f = fopen(config_path, "r");
    if (!f) {
        log_message("Error: Cannot open config file");
        return;
    }
    child_count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f) && child_count < MAX_CHILDREN) {
        if (line[0] == '#' || strlen(line) < 5) continue;
        
        ChildConfig tmp;
        if (sscanf(line, "%255s %255s %255s %255s", tmp.path, tmp.arg, tmp.in, tmp.out) == 4) {
            if (tmp.path[0] != '/' || tmp.in[0] != '/' || tmp.out[0] != '/') {
                log_message("Skip: Only absolute paths allowed!");
                continue;
            }
            children[child_count] = tmp;
            start_child(child_count);
            child_count++;
        }
    }
    fclose(f);
}

void handle_sighup(int sig) {
    (void)sig;
    reload_config = 1;
}

int main(int argc, char **argv) {
    int opt;
    char *cfg_arg = NULL;

    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
            case 'c': cfg_arg = optarg; break;
            default: exit(EXIT_FAILURE);
        }
    }

    if (!cfg_arg) {
        fprintf(stderr, "Error: Config file required (-c)\n");
        exit(EXIT_FAILURE);
    }

    if (realpath(cfg_arg, config_path) == NULL) {
        perror("Error resolving config path");
        exit(EXIT_FAILURE);
    }

    if (getppid() != 1) {
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        if (fork() != 0) exit(0);
        if (setsid() < 0) exit(EXIT_FAILURE);
    }

    struct rlimit flim;
    if (getrlimit(RLIMIT_NOFILE, &flim) == 0) {
        for (int i = 0; i < (int)flim.rlim_max; i++) close(i);
    }

    if (chdir("/") != 0) exit(EXIT_FAILURE);

    signal(SIGHUP, handle_sighup);
    log_message("daemon started");
    read_config();

    while (1) {
        if (reload_config) {
            log_message("Reloading...");
            for (int i = 0; i < child_count;i++) {
                if (children[i].pid > 0) kill(children[i].pid, SIGTERM);
            }
            reload_config = 0;
            read_config();
        }

        int status;
        pid_t exited_pid = waitpid(-1, &status, WNOHANG);
        if (exited_pid > 0) {
            for (int i = 0; i < child_count; i++) {
                if (children[i].pid == exited_pid) {
                    char msg[512];
                    snprintf(msg, sizeof(msg), "PID %d exited (code %d). Restarting...", exited_pid, status);
                    log_message(msg);
                    start_child(i);
                    break;
                }
            }
        }
        sleep(1);
    }
    return 0;
}
