#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define PORT "6776"

void clean_buffer(char buffer[], int size) {
    for (int i = 0; i < size; i++) {
        if (buffer[i] < 32 || buffer[i] > 126) {
            for (int j = i; j < size; j++) {
                buffer[j] = buffer[j+1];
                size = size - 1;
            }
        }
    }
    buffer[size] = '\0';
}

int main(void) {

    //Phase one: listener

    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res, *p;
    int sockfd, yes=1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, PORT, &hints, &res);

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            continue;
        }

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind");
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        return -1;
    }

    listen(sockfd, 10);

    printf("Listening on port %s\n", PORT);

    //Phase two: poll

    char buffer[1024] = {0};

    struct pollfd pfds[6];
    struct sockaddr_in ips[6];

    pfds[0].fd = sockfd;
    pfds[0].events = POLLIN;

    int index = 1;

    while (1) {
        poll(pfds, index, -1);

        for (int i = 0; i < index; i++) {
            if (i == 0) {
                if (pfds[0].revents & POLLIN) {
                    int newfd = accept(sockfd, (struct sockaddr *) &their_addr, &addr_size);
                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        pfds[index].fd = newfd;
                        pfds[index].events = POLLIN;
                        char ipstr[INET6_ADDRSTRLEN];
                        struct sockaddr_in *sin = (struct sockaddr_in *)&their_addr;
                        inet_ntop(AF_INET, &sin->sin_addr, ipstr, sizeof ipstr);
                        ips[index] = *sin;
                        index++;
                        printf("New connection: Client %d at %s\n", index-1, ipstr);
                    }
                }
                continue;
            }
            if (!(pfds[i].revents & POLLIN)) {
                continue;
            }
            //printf("Client %d at %s is ready to read\n", i, inet_ntoa(ips[i].sin_addr));

            int bytes_read = recv(pfds[i].fd, buffer, sizeof(buffer), 0);

            switch (bytes_read) {
                case -1:
                    perror("recv");
                    break;
                case 0:
                    close(pfds[i].fd);
                    printf("Connection closed, new length is %d\n", index-1);
                    pfds[index-1] = pfds[i];
                    index--;
                    break;
                default:
                    clean_buffer(buffer, bytes_read);
                printf("Client %d at %s said: %s\n", i, inet_ntoa(ips[i].sin_addr), buffer);
                for (int j = 1; j < index; j++) {
                    if (i != j) {
                        int bytes_sent = send(pfds[j].fd, buffer, bytes_read, 0);
                        printf("Sent %d bytes to client %d at %s\n", bytes_sent, j, inet_ntoa(ips[j].sin_addr));
                    }
                }
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
