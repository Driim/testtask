#include <stdio.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

int main(int argc, const char* argv[])
{
    int out_fd, in_fd, port, ret;
    struct sockaddr_in server;
    struct stat st;
    ssize_t sended, all = 0;
    off_t offset = 0;

    if(argc < 3) {
        fprintf(stdout, "Usage: client ip_addres port filename\n");
        fprintf(stdout, "\tip_addres - servers ip address\n");
        fprintf(stdout, "\tport - server port\n");
        fprintf(stdout, "\tfilename - file that will be send\n");
        exit(EXIT_FAILURE);
    }

    ret = stat(argv[3], &st);
    if(ret < 0) {
        perror("Failed to stat file:");
        exit(EXIT_FAILURE);
    }

    in_fd = open(argv[3], O_RDONLY);
    if(in_fd < 0) {
        fprintf(stderr, "Failed to open file %s", argv[3]);
        exit(EXIT_FAILURE);
    }

    out_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(out_fd < 0) {
        fprintf(stderr, "Failed to create socket");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[2]);

    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    ret = connect(out_fd, (struct sockaddr *)&server, sizeof(server));
    if (ret < 0) {
        perror("Failed to connect:");
        exit(EXIT_FAILURE);
    }

    /*
     * Никаких специфичных требований к клиенту не было и было разрешено использовать специфичные
     * для линукса вещи, по этому я принял решение использовать наиболее эффективный способ отправки
     * файла одним системным вызовом. По хорошему стоит еще реализовать механизм fallback to read/write.
     */
    while(1) {
        sended = sendfile(out_fd, in_fd, &offset, st.st_size - offset);
        if(sended < 0) {
            perror("Failed to send file:");
            exit(EXIT_FAILURE);
        }
        all += sended;
        if(all == st.st_size)
            break;
    }

    exit(EXIT_SUCCESS);
}