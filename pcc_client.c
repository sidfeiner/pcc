#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <memory.h>
#include <arpa/inet.h>
#include <zconf.h>

#define READ_BATCH_SIZE 500

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


int getFileSize(FILE *file) {
    long size;
    fseek(file, 0L, SEEK_END);
    size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return (int)size;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "wrong number of arguments give\n");
        exit(1);
    }
    char *ipAddress = argv[1];
    int16_t port;
    size_t fileSize;
    int netPrintableCharsAmount;
    if ((port = parse_port(argv[2])) <= 0) {
        fprintf(stderr, "port must be a positive number\n");
        exit(1);
    }
    char *filePath = argv[3];
    FILE *file;
    if ((file = fopen(filePath, "r")) == NULL) {
        fprintf(stderr, "failed opening file\n");
        exit(1);
    }

    printf("reading file...\n");
    char *fileContent;
    fileSize = getFileSize(file);
    fileContent = malloc(sizeof(char )*fileSize);
    fread(fileContent, sizeof(char), fileSize, file);
    printf("successfully read file with %lu bytes\n", fileSize);

    int sockfd = -1;
    struct sockaddr_in serv_addr; // where we Want to get to


    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Error : Could not create socket \n");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); // Note: htons for endiannes
    serv_addr.sin_addr.s_addr = inet_addr(ipAddress); // hardcoded...

    if (connect(sockfd,
                (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) < 0) {
        printf("\n Error : Connect Failed to port %d : %s \n", ntohs(serv_addr.sin_port), strerror(errno));
        exit(1);
    }

    int fileSizeInNet = htonl(fileSize);
    write(sockfd, &fileSizeInNet, sizeof(int));
    write(sockfd, fileContent, fileSize);
    read(sockfd, &netPrintableCharsAmount, sizeof(int));
    printf("# of printable characters: %u\n", ntohl(netPrintableCharsAmount));
    close(sockfd);
    return 0;
}