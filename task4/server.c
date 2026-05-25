#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

#define MAX_CONNECTIONS 1024
#define IO_BUFFER_SIZE 8192

struct SessionContext {
    char data_buffer[IO_BUFFER_SIZE];
    int data_length;
};

long long server_state = 0;
struct pollfd monitored_fds[MAX_CONNECTIONS];
struct SessionContext sessions[MAX_CONNECTIONS];
int active_fds = 0;

void configure_nonblocking(int descriptor) {
    int current_flags = fcntl(descriptor, F_GETFL, 0);
    fcntl(descriptor, F_SETFL, current_flags | O_NONBLOCK);
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    
    FILE *config_fp = fopen("config", "r");
    if (!config_fp) return 1;
    
    char socket_path[108];
    
    if (fscanf(config_fp, "%107s", socket_path) != 1) {
        fclose(config_fp);
        return 1;
    }
    fclose(config_fp);

    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    unlink(socket_path);
    
    struct sockaddr_un server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sun_family = AF_UNIX;
    
    snprintf(server_address.sun_path, sizeof(server_address.sun_path), "%s", socket_path);
    
    bind(listen_fd, (struct sockaddr*)&server_address, sizeof(server_address));
    listen(listen_fd, MAX_CONNECTIONS);
    configure_nonblocking(listen_fd);

    monitored_fds[0].fd = listen_fd;
    monitored_fds[0].events = POLLIN;
    active_fds = 1;

    FILE *log_fp = fopen("/tmp/server.log", "w");
    if (log_fp) fclose(log_fp);

    while (1) {
        if (poll(monitored_fds, active_fds, -1) <= 0) {
            continue;
        }

        for (int idx = 0; idx < active_fds; idx++) {
            if (monitored_fds[idx].revents & (POLLIN | POLLHUP | POLLERR)) {
                
                if (monitored_fds[idx].fd == listen_fd) {
                    int client_fd = accept(listen_fd, NULL, NULL);
                    if (client_fd >= 0 && active_fds < MAX_CONNECTIONS) {
                        configure_nonblocking(client_fd);
                        monitored_fds[active_fds].fd = client_fd;
                        monitored_fds[active_fds].events = POLLIN;
                        sessions[active_fds].data_length = 0;
                        active_fds++;
                        
                        log_fp = fopen("/tmp/server.log", "a");
                        if (log_fp) { 
                            fprintf(log_fp, "ACCEPTED FD: %d\n", client_fd); 
                            fclose(log_fp); 
                        }
                    }
                } else {
                    char temporary_buf[IO_BUFFER_SIZE];
                    ssize_t read_bytes = read(monitored_fds[idx].fd, temporary_buf, sizeof(temporary_buf));
                    
                    if (read_bytes > 0) {
                        int free_space = IO_BUFFER_SIZE - sessions[idx].data_length;
                        int bytes_to_move = read_bytes < free_space ? read_bytes : free_space;
                        memcpy(sessions[idx].data_buffer + sessions[idx].data_length, temporary_buf, bytes_to_move);
                        sessions[idx].data_length += bytes_to_move;

                        char *newline_ptr;
                        while ((newline_ptr = memchr(sessions[idx].data_buffer, '\n', sessions[idx].data_length)) != NULL) {
                            int token_len = newline_ptr - sessions[idx].data_buffer;
                            char command_chunk[32] = {0};
                            memcpy(command_chunk, sessions[idx].data_buffer, token_len < 31 ? token_len : 31);
                            
                            server_state += strtoll(command_chunk, NULL, 10);
			    char response_buffer[64];
                            int response_bytes = snprintf(response_buffer, sizeof(response_buffer), "%lld\n", server_state);
                            
                            if (write(monitored_fds[idx].fd, response_buffer, response_bytes) < 0) {
                            }

                            int leftovers = sessions[idx].data_length - (token_len + 1);
                            memmove(sessions[idx].data_buffer, newline_ptr + 1, leftovers);
                            sessions[idx].data_length = leftovers;
                        }
                    }
                    
                    if (read_bytes == 0 || (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) || (monitored_fds[idx].revents & (POLLHUP | POLLERR))) {
                        close(monitored_fds[idx].fd);
                        monitored_fds[idx] = monitored_fds[active_fds - 1];
                        sessions[idx] = sessions[active_fds - 1];
                        active_fds--;
                        idx--; 
                    }
                }
            }
        }
    }
    return 0;
}
