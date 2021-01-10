#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <signal.h>

#define PCC_SIZE (126 - 32 + 1)
#define QUEUE_SIZE 10
#define FAIL (-1)

int pccTotal[PCC_SIZE]; // counter for each printable char
int listenFd = -1;
int connFd = -1;
int isDone = 0;

/**
 * @param strPort port from input
 * @param intPort where to store the int result
 * @return -1 if conversion failed, 0 if successfull
 */
int16_t parse_port(char *strPort) {
    int port = (int16_t) strtol(strPort, NULL, 10);
    if (port == UINT_MAX && errno == ERANGE) {
        return -1;
    }
    return port;
}

/**
 * Increments the global counter for every printable char in input
 * returns the total amount of printable chars
 */
int incPrintableChars(char *chars, int size) {
    char *tmp, *limit;
    int cnt = 0;
    limit = chars + size;
    for (tmp = chars; tmp < limit; tmp++) {
        if (*tmp >= 32 && *tmp <= 126) {
            pccTotal[*tmp - 32] += 1;
            cnt++;
        }
    }
    return cnt;
}

void readFromSocket(int fd, void *buffer, size_t expectedSize) {
    size_t curRead, readAmt = 0;
    while (readAmt < expectedSize) {
        curRead = read(fd, buffer, expectedSize);
        expectedSize -= curRead;
        buffer += curRead;
    }
}

void shutdown_server(int sig) {
    int *tmpChar, *limit, index;
    isDone = 1;
    if (connFd > -1) {
        close(connFd);
    }
    if (listenFd > -1) {
        close(listenFd);
    }
    limit = pccTotal + PCC_SIZE;
    for (index = 0, tmpChar = pccTotal; tmpChar < limit; tmpChar++, index++) {
        printf("char '%c' : %u times\n", index + 32, *tmpChar);
    }
}

int initSigintHandler() {
    struct sigaction sa;
    sa.sa_handler = shutdown_server;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    return sigaction(SIGINT, &sa, NULL);
}

void blockSignal(sigset_t *blockSet) {
    sigemptyset(blockSet);
    sigaddset(blockSet, SIGINT);
    sigprocmask(SIG_BLOCK, blockSet, NULL);
}

void unblockSignal(sigset_t blockSet) {
    sigprocmask(SIG_UNBLOCK, &blockSet, NULL);  // Now pid has been persisted, handle SIGINT if needed
}

int isSocketError() {
    return errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE;
}

int handleConnection() {
    int printableChars, netPrintableChars, netBufferSize, bufferSize;
    char *buffer;
    readFromSocket(connFd, &netBufferSize, sizeof(int));
    printf("done reading size\n");
    if (isSocketError()) {
        fprintf(stderr, "failed reading file size from socket\n");
        return FAIL;
    }
    bufferSize = ntohl(netBufferSize);
    buffer = malloc(sizeof(char) * bufferSize);
    readFromSocket(connFd, buffer, bufferSize);
    printf("done reading file\n");
    if (isSocketError()) {
        fprintf(stderr, "failed reading file from socket\n");
        return FAIL;
    }
    printableChars = incPrintableChars(buffer, bufferSize);
    netPrintableChars = htonl(printableChars);
    write(connFd, &netPrintableChars, sizeof(int));
    if (isSocketError()) {
        fprintf(stderr, "failed writing printable chars to socket\n");
        return FAIL;
    }
    free(buffer);
    return 0;
}


int runServer(int16_t port) {
    sigset_t blockSet;
    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);

    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt(SO_REUSEADDR) failed\n");
        return 1;
    }

    memset(&serv_addr, 0, addrsize);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listenFd, (struct sockaddr *) &serv_addr, addrsize) != 0) {
        fprintf(stderr, "Error : Bind Failed. %s \n", strerror(errno));
        return 1;
    }

    if (listen(listenFd, QUEUE_SIZE) != 0) {
        fprintf(stderr, "Error : Listen Failed. %s \n", strerror(errno));
        return 1;
    }

    while (!isDone) {
        connFd = accept(listenFd, (struct sockaddr *) &peer_addr, &addrsize);
        if (isDone) {
            return 0;
        } else if (connFd < 0) {
            fprintf(stderr, "Error : Accept Failed. %s \n", strerror(errno));
            return 1;
        }
        blockSignal(&blockSet);
        handleConnection();
        close(connFd);
        connFd = -1;
        unblockSignal(blockSet);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "wrong number of arguments give\n");
        exit(1);
    }
    int16_t port;
    if ((port = parse_port(argv[1])) <= 0) {
        fprintf(stderr, "port must be a positive number\n");
        exit(1);
    }

    initSigintHandler();

    return runServer(port);
}