#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#define DEFAULT_BLOCK_SIZE 4096

int is_all_zeros(const char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (buf[i] != 0) return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    int block_size = DEFAULT_BLOCK_SIZE;
    int opt;

    while ((opt = getopt(argc, argv, "b:")) != -1) {
        switch (opt) {
            case 'b':
                block_size = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Using: %s [-b block_size] [input_file] output_file\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    int in_fd = STDIN_FILENO;
    char *out_filename = NULL;

    if (argc - optind == 1) {
        out_filename = argv[optind];
    } else if (argc - optind == 2) {
        in_fd = open(argv[optind], O_RDONLY);
        if (in_fd < 0) { perror("Error: open input_file"); exit(1); }
        out_filename = argv[optind + 1];
    } else {
        fprintf(stderr, "Error: not specified output_file.\n");
        exit(1);
    }

    int out_fd = open(out_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) { perror("Error: create output_file"); exit(1); }

    char *buffer = malloc(block_size);
    ssize_t bytes_read;
    off_t total_size = 0;

    while ((bytes_read = read(in_fd, buffer, block_size)) > 0) {
        if (is_all_zeros(buffer, bytes_read)) {
             //  блок нулей, смещаем указатель в итоговом файле
            if (lseek(out_fd, bytes_read, SEEK_CUR) == -1) {
                perror("lseek error");
                exit(1);
            }
        } else {
            if (write(out_fd, buffer, bytes_read) != bytes_read) {
                perror("write error");
                exit(1);
            }
        }
        total_size += bytes_read;
    }
    // устанавливаем итоговый размер файла, если последний блок был с дыркой
    if (ftruncate(out_fd, total_size) == -1) {
        perror("ftruncate error");
    }

    free(buffer);
    close(in_fd);
    close(out_fd);
    return 0;
}
