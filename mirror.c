//
// Created by v2 on 11/04/23.
//
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <malloc.h>
#include <stdlib.h>

#define SERVER_PORT 8080
#define MIRROR_PORT 8081
#define NULL_CH '\0'

int SOCKET_WAITING_QUEUE_SIZE = 3;

char *getCurrentHostIp() {
    int IP_LENGTH = 25;
    char hostName[256];
    struct hostent *hostData;
    char *ip = malloc(IP_LENGTH * sizeof(char));

    if (gethostname(hostName, sizeof(hostName)) >= 0) {
        hostData = gethostbyname(hostName);
        char *data = inet_ntoa(*((struct in_addr *) hostData->h_addr_list[0]));
        for (int i = 0; i < IP_LENGTH; i++) ip[i] = NULL_CH;
        strcpy(ip, data);
    }
    return ip;
}

int registerOntoServer(char *hostIp, char *serverIp) {
    int ipLength = (int) strlen(hostIp);
    char initMsg[31] = {NULL_CH};
    strncpy(initMsg, "Mir=", 4);
    int i = 0;
    for (; i < ipLength; i++) initMsg[i + 4] = hostIp[i];
    initMsg[i + 4] = ';';
    printf("initmsg => %s, length: %lu\n", initMsg, strlen(initMsg));

    int fd_mirror_client;
    long int bytesReceived;
    struct sockaddr_in serv_addr;

    char buffer[1024] = {0};
    if ((fd_mirror_client = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        exit(-1);
    }
    printf("Socket creation complete for server registration...\n");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, serverIp, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        exit(-1);
    }

    printf("Attempting server registration now...\n");
    if ((connect(fd_mirror_client, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
        printf("\nConnection Failed \n");
        exit(-1);
    }

    printf("Initiating server registration...\n");
    send(fd_mirror_client, initMsg, strlen(initMsg), 0);
    bytesReceived = read(fd_mirror_client, buffer, 1024);
    close(fd_mirror_client);

    if (strcmp(buffer, "OK") == 0) {
        printf("Server registration completed, ready for handling client requests now...\n");
        return 0;
    }
    printf("Server responded with %ld bytes of msg: '%s'... Registration failed..\n", bytesReceived, buffer);
    return -1;
}

void processClient(int socket, char buffer[], long int bytesRead) {
    int cid = fork();
    if (cid == 0) {
        printf("%d => %ld bytes read. String => '%s'\n", getpid(), bytesRead, buffer);
        char *clientMessage = "Monetary case from mirror";
        send(socket, clientMessage, strlen(clientMessage), 0);
        printf("Hello message sent\n");

        // closing the connected socket
        close(socket);
        exit(0);
    } else {
        printf("from the root process.. nothing else to do?\n");
    }
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Cannot start mirror. Need server's IP address as the 1st parameter...\n");
        exit(0);
    }
    char *serverIp = argv[1];

    char *ip = getCurrentHostIp();
    printf("The current mirror host ip is: %s\n", ip);
    if (registerOntoServer(ip, serverIp) < 0) {
        exit(-1);
    }

    int server_fd, socketConn;
    long int bytesRead;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //AF_INET for IPv4, SOCK_STREAM -- TCP
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MIRROR_PORT);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, SOCKET_WAITING_QUEUE_SIZE) < 0) { //limiting to 3 requests in the queue
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Entering listening state now...\n");
    while (1) {
        printf("Waiting for accepting..\n");
        if ((socketConn = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        char buffer[1024] = {0};

        bytesRead = read(socketConn, buffer, 1024);
        printf("Accepted a request from the queue..\n");
        processClient(socketConn, buffer, bytesRead);
    }
    return 0;
}