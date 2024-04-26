#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

// slim TCP protocol
// https://wiki.slimdevices.com/index.php/SlimProto_TCP_protocol

// Squezebox Server Port
#define PORT 3483
#define BUFFER_SIZE 1024

// Radio Caroline
#define STREAM_IP {78, 129, 202, 10}
#define STREAM_PORT 8040
#define STREAM_PATH "GET /stream.mp3 HTTP/1.0\r\n\r\n"

volatile uint8_t keep_running = 1;

void sigint_handler(int sig) {
    keep_running = 0;
}


void send_stream_command(int socket_fd) {
    unsigned char ip[4] = STREAM_IP;
    int port = STREAM_PORT;
    char* path = STREAM_PATH;

    unsigned char buffer[BUFFER_SIZE];
    int data_len;

    // cmd STRMS
    buffer[2] = 's'; buffer[3] = 't'; buffer[4] = 'r'; buffer[5] = 'm'; buffer[6] = 's'; 

    // autostart, formatbyte, pcmsampsize, pcmrate, channels, endian
    buffer[7] = '1'; buffer[8] = 'm'; buffer[9] = '?'; buffer[10] = '?'; buffer[11] = '?'; buffer[12] = '?';

    // threshold, spdif_enable, trans_period, trans_type, flags, output thresh, reserved
    buffer[13] = 255; buffer[14] = 0; buffer[15] = 10; buffer[16] = 48; buffer[17] = 0; buffer[18] = 0; buffer[19] = 0;

    // gain
    buffer[20] = 0; buffer[21] = 0; buffer[22] = 0; buffer[23] = 0;

    // port
    buffer[24] = port/256; buffer[25] = port%256;

    // ip
    buffer[26] = ip[0]; buffer[27] = ip[1]; buffer[28] = ip[2]; buffer[29] = ip[3];

    strcpy(buffer + 30, path);

    data_len = 30 + strlen(path);
    buffer[0] = data_len/256; buffer[1] = data_len%256;

    write(socket_fd, buffer, data_len + 2);
}

void handle_helo(int socket_fd, unsigned char* data_buffer, unsigned int data_length) {
    send_stream_command(socket_fd);
}

void handle_stat(int socket_fd, unsigned char* data_buffer, unsigned int data_length) {
    // noop
}

int read_exact_bytes(int socket_fd, unsigned char* buffer, int bytes_to_read) {
    int total_read = 0;
    while (total_read < bytes_to_read) {
        int bytes_read = recv(socket_fd, buffer + total_read, bytes_to_read - total_read, 0);
        if (bytes_read <= 0) {
            // Error or connection closed
            return -1;
        }
        total_read += bytes_read;
    }
    return total_read;
}

void handle_connection(int socket_fd) {
    unsigned char command_buffer[8];
    unsigned char data_buffer[BUFFER_SIZE];
    unsigned int data_length = 0;


    while (keep_running) {
        // Read command and length
        if (read_exact_bytes(socket_fd, command_buffer, 8) <= 0) {
            perror("Failed to read command or connection closed");
            break;
        }

        for (int i = 4; i < 8; i++) {
            data_length = (data_length << 8) + command_buffer[i];
        }

        if (data_length > BUFFER_SIZE) {
            printf("Data length too large\n");
            break;
        }

        // Read data based on length
        if (read_exact_bytes(socket_fd, data_buffer, data_length) <= 0) {
            perror("Failed to read data or connection closed");
            break;
        }

        // Dispatch command
        if (memcmp(command_buffer, "HELO", 4) == 0) {
            handle_helo(socket_fd, data_buffer, data_length);
        } else if (memcmp(command_buffer, "STAT", 4) == 0) {
            handle_stat(socket_fd, data_buffer, data_length);
        } else {
            printf("Unhandled command\n");
        }
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Setup signal handler
    signal(SIGINT, sigint_handler);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connections...\n");

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // Handle the connection
    handle_connection(new_socket);

    // Close the socket after handling the connection
    close(new_socket);

    return 0;
}
