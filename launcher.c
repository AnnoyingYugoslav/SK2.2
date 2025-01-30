#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define UDP_PORT 1111
#define BUFFER_SIZE 256

int start_tcp_server(int port) {
    int server_fd;
    struct sockaddr_in address;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("TCP socket failed");
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("TCP bind failed");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 3) < 0) {
        perror("TCP listen failed");
        close(server_fd);
        return -1;
    }

    printf("TCP server started on port %d\n", port);
    return server_fd;
}

int create_udp_socket(int port) {
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == -1) {
        perror("Cannot create UDP socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in localAddress;
    memset(&localAddress, 0, sizeof(localAddress));
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(port);
    localAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_socket, (struct sockaddr*)&localAddress, sizeof(localAddress)) < 0) {
        perror("UDP bind failed");
        close(udp_socket);
        exit(EXIT_FAILURE);
    }

    return udp_socket;
}

int main() {
    struct sockaddr_in clientAddress;
    int udp_socket, tcp_socket, new_udp_socket;
    socklen_t len = sizeof(clientAddress);
    char buffer[BUFFER_SIZE];

    udp_socket = create_udp_socket(UDP_PORT);
    printf("UDP server listening on port %d\n", UDP_PORT);

    for (;;) {
        memset(buffer, 0, BUFFER_SIZE);
        if (recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&clientAddress, &len) < 0) {
            perror("UDP receive failed");
            continue;
        }

        int tcp_port = atoi(buffer);
        if (tcp_port <= 0 || tcp_port > 65535) {
            printf("Invalid TCP port received: %s\n", buffer);
            continue;
        }

        tcp_socket = start_tcp_server(tcp_port);
        if (tcp_socket == -1) {
            continue;
        }

        int new_udp_port = 20000 + (rand() % 10000);
        char response[32];
        snprintf(response, sizeof(response), "%d", new_udp_port);

        if (sendto(udp_socket, response, strlen(response), 0, (struct sockaddr*)&clientAddress, len) < 0) {
            perror("UDP send failed");
        } else {
            printf("Sent new UDP port %d to client\n", new_udp_port);
        }

        close(udp_socket);
        new_udp_socket = create_udp_socket(new_udp_port);

        memset(buffer, 0, BUFFER_SIZE);
    }

    close(udp_socket);
    return 0;
}
