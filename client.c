#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET Socket;
#define BAD_SOCKET INVALID_SOCKET
#define close_socket closesocket
#define SEND_FLAGS 0
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int Socket;
#define BAD_SOCKET (-1)
#define close_socket close
#define SEND_FLAGS MSG_NOSIGNAL
#endif

enum { MAX_NUMBER = 10 };

static int parse_port(const char *text)
{
    char *end;
    long port = strtol(text, &end, 10);
    return *end == '\0' && port > 0 && port <= 65535 ? (int)port : 0;
}

static int start_network(void)
{
#ifdef _WIN32
    WSADATA data;
    int error = WSAStartup(MAKEWORD(2, 2), &data);
    if (error)
        fprintf(stderr, "WSAStartup: %d\n", error);
    return error;
#else
    return 0;
#endif
}

static void stop_network(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static void socket_error(const char *operation)
{
#ifdef _WIN32
    fprintf(stderr, "%s: Winsock error %d\n", operation, WSAGetLastError());
#else
    perror(operation);
#endif
}

static int parse_address(const char *text, struct sockaddr_in *address)
{
    const char *separator = strchr(text, ':');
    char ip[INET_ADDRSTRLEN];
    if (!separator || separator - text >= (int)sizeof ip)
        return 0;

    int port = parse_port(separator + 1);
    size_t length = (size_t)(separator - text);
    memcpy(ip, text, length);
    ip[length] = '\0';
    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    return port && inet_pton(AF_INET, ip, &address->sin_addr) == 1;
}

static int play(Socket server)
{
    for (char guess = 1; guess <= MAX_NUMBER; ++guess) {
        char correct;
        if (send(server, &guess, 1, SEND_FLAGS) != 1 ||
            recv(server, &correct, 1, 0) != 1)
            break;
        printf("Guess %d - %s\n", guess, correct ? "correct" : "wrong");
        if (correct)
            return 0;
    }
    fprintf(stderr, "Game ended without a correct guess\n");
    return 1;
}

int main(int argc, char **argv)
{
    if (start_network() != 0)
        return 1;

    struct sockaddr_in address = {0};
    if (argc != 2 || !parse_address(argv[1], &address)) {
        fprintf(stderr, "Usage: %s IPv4:PORT\n", argv[0]);
        stop_network();
        return 1;
    }

    Socket server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == BAD_SOCKET) {
        socket_error("socket");
        stop_network();
        return 1;
    }
    int result = 1;
    if (connect(server, (struct sockaddr *)&address, sizeof address) == 0)
        result = play(server);
    else
        socket_error("connect");

    close_socket(server);
    stop_network();
    return result;
}
