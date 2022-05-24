#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

struct Pipe {
    int fd_send;
    int fd_recv;
};


void *handle_chat(void *data) {
    struct Pipe *pipe = (struct Pipe *) data;
    char buffer[1024] = "Message: ";
    char tmp_buffer[1024] = "Message: "; // 处理换行时，分割后的信息拷贝到 tmp_buffer 里
    ssize_t len;
    int remain = 8;
    char *pos = NULL;
    while ((len = recv(pipe->fd_send, buffer + remain, 1000, 0)) > 0) {
        while ((pos = strchr(buffer, '\n')) != NULL) {
            strncpy(tmp_buffer + 8, buffer + 8, pos - buffer + 1 - 8);
            tmp_buffer[pos - buffer + 2] = '\0';
            send(pipe->fd_recv, tmp_buffer, strlen(tmp_buffer), 0);
            remain = len - (pos - buffer - 8 + 1);
            strncpy(tmp_buffer + 8, buffer + (pos + 1 - buffer), remain);
            tmp_buffer[pos + 1 - buffer + remain + 1] = '\0';
            strncpy(buffer, tmp_buffer, 1024);
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    if (bind(fd, (struct sockaddr *) &addr, addr_len)) {
        perror("bind");
        return 1;
    }
    if (listen(fd, 2)) {
        perror("listen");
        return 1;
    }
    int fd1 = accept(fd, NULL, NULL);
    int fd2 = accept(fd, NULL, NULL);
    if (fd1 == -1 || fd2 == -1) {
        perror("accept");
        return 1;
    }
    pthread_t thread1, thread2;
    struct Pipe pipe1;
    struct Pipe pipe2;
    pipe1.fd_send = fd1;
    pipe1.fd_recv = fd2;
    pipe2.fd_send = fd2;
    pipe2.fd_recv = fd1;
    pthread_create(&thread1, NULL, handle_chat, (void *) &pipe1);
    pthread_create(&thread2, NULL, handle_chat, (void *) &pipe2);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    return 0;
}
