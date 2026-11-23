#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static void play(Socket client, const char *ip)
{
    int secret = 1 + rand() % MAX_NUMBER;
    printf("%s: Game started (1-%d)\n", ip, MAX_NUMBER);

    char guess;
    while (recv(client, &guess, 1, 0) == 1) {
        char correct = guess == secret;
        printf("%s: Guess %d - %s\n", ip, guess,
               correct ? "correct" : "wrong");
        if (send(client, &correct, 1, SEND_FLAGS) != 1 || correct)
            break;
    }
}

int main(int argc, char **argv)
{
    int port = argc == 2 ? parse_port(argv[1]) : 0;
    if (!port) {
        fprintf(stderr, "Usage: %s PORT\n", argv[0]);
        return 1;
    }
    if (start_network() != 0)
        return 1;

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    Socket server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == BAD_SOCKET) {
        socket_error("socket");
        stop_network();
        return 1;
    }
    if (bind(server, (struct sockaddr *)&address, sizeof address) == 0 &&
        listen(server, SOMAXCONN) == 0) {
        setvbuf(stdout, NULL, _IONBF, 0);
        srand((unsigned int)time(NULL));
        printf("Listening on port %d\n", port);

        for (;;) {
            struct sockaddr_in peer;
            socklen_t length = sizeof peer;
            Socket client = accept(server, (struct sockaddr *)&peer, &length);
            if (client == BAD_SOCKET) {
                socket_error("accept");
                break;
            }
            const char *ip = inet_ntoa(peer.sin_addr);
            printf("%s: Connected\n", ip);
            play(client, ip);
            close_socket(client);
            printf("%s: Disconnected\n", ip);
        }
    } else {
        socket_error("bind/listen");
    }

    close_socket(server);
    stop_network();
    return 1;
}
