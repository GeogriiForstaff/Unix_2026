#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;

    FILE *config_file = fopen(argv[1], "r");
    if (!config_file) return 1;
    char socket_path[256];
    
    if (fscanf(config_file, "%255s", socket_path) != 1) {
        fclose(config_file);
        return 1;
    }
    fclose(config_file);

    double upper_delay_bound = atof(argv[3]);
    
    int conn_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un remote_address = { .sun_family = AF_UNIX };
    strcpy(remote_address.sun_path, socket_path);
    if (connect(conn_socket, (struct sockaddr*)&remote_address, sizeof(remote_address)) == -1) {
        return 1;
    }

    int socket_flags = fcntl(conn_socket, F_GETFL, 0);
    fcntl(conn_socket, F_SETFL, socket_flags | O_NONBLOCK);

    FILE *input_data_fp = fopen(argv[2], "r");
    if (!input_data_fp) return 1;
    
    srand((unsigned int)time(NULL) + (unsigned int)getpid());
    int sent_bytes_counter = 0;
    int bytes_until_pause = rand() % 255 + 1;
    double accumulated_delay = 0;

    char response_line[256] = {0};
    int line_pos = 0;
    char latest_server_response[64] = "0";
    char read_buffer[1024];
    int read_result;

    int input_char;
    while ((input_char = fgetc(input_data_fp)) != EOF) {
        char character_to_send = (char)input_char;
        while (write(conn_socket, &character_to_send, 1) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
            } else {
                goto finish_transmission;
            }
        }
        
        if (++sent_bytes_counter == bytes_until_pause) {
            double current_sleep = ((double)rand() / RAND_MAX) * upper_delay_bound;
            usleep(current_sleep * 1000000);
            accumulated_delay += current_sleep;
            sent_bytes_counter = 0;
            bytes_until_pause = rand() % 255 + 1;
        }

        while ((read_result = read(conn_socket, read_buffer, sizeof(read_buffer))) > 0) {
            for (int idx = 0; idx < read_result; idx++) {
                if (read_buffer[idx] == '\n') {
                    response_line[line_pos] = '\0';
                    strcpy(latest_server_response, response_line);
                    line_pos = 0;
                } else if (line_pos < 255) {
                    response_line[line_pos++] = read_buffer[idx];
                }
            }
        }
    }

finish_transmission:
    fclose(input_data_fp);

    int clean_flags = fcntl(conn_socket, F_GETFL, 0);
    fcntl(conn_socket, F_SETFL, clean_flags & ~O_NONBLOCK);
    shutdown(conn_socket, SHUT_WR);
    
    while ((read_result = read(conn_socket, read_buffer, sizeof(read_buffer))) > 0) {
        for (int idx = 0; idx < read_result; idx++) {
            if (read_buffer[idx] == '\n') {
                response_line[line_pos] = '\0';
                strcpy(latest_server_response, response_line);
                line_pos = 0;
            } else if (line_pos < 255) {
                response_line[line_pos++] = read_buffer[idx];
            }
        }
    }
    close(conn_socket);

    fprintf(stderr, "LAST_RESP:%s\n", latest_server_response);
    printf("%f\n", accumulated_delay);
    return 0;
}
