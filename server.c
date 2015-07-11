#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include <event2/event.h>
#include <event2/event_compat.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <fcntl.h>
#include <unistd.h>
#include <event2/buffer.h>

/*
 *
 * Событийно-ориентированный сервер реализованный при помощи библиотеки libevent2.
 * Такая реализация является оптимальной для случая большого количества параллейных соединений
 * так как не плодит потоки занимающие ЦП и оперативную память. А для работы с файлами
 * libevent2 применяет sendfile или iovec(векторизированный ввод/вывод), в зависимости от системы
 * и флагов компиляции библиотеки.
 *
 */

/* Просто счетчик для генерации имени файла */
volatile static int file_counter;

static void error_callback(struct bufferevent *bev, short error, void *ctx)
{
    int *fd = ctx;

    /*
     * Возможно 3 вариант, либо был EOF от сокета, либо ошибка,
     * либо таймаут, по хорошему во 2 и 3 случае стоит удалить
     * созданный файл, но для простоты этого не делал, так что
     * обработчик для всех 3 случаев одинаковый.
     */
    close(*fd);
    free(fd);
    bufferevent_free(bev);

}

static void socket_read_cb(struct bufferevent *bev, void *ctx)
{
    size_t ret;
    int *fd = ctx;

    struct evbuffer *buffer = bufferevent_get_input(bev);
    evbuffer_write(buffer, *fd);
}

static void accept_connection_cb(struct evconnlistener *listener,
                                 evutil_socket_t fd, struct sockaddr *addr, int sock_len,
                                 void *arg)
{
    struct bufferevent *ev_buf;
    struct timeval timeout;
    int ret;
    int *file_fd;

    /* создаем файл для записи */
    char path[100];
    sprintf(path, "%s/%d.txt", (char *)arg, file_counter++);
    file_fd =malloc(sizeof(int));
    *file_fd = open(path, O_WRONLY | O_CREAT);

    /* Создаем сокет для подключения */
    ev_buf = bufferevent_socket_new(evconnlistener_get_base(listener), fd, BEV_OPT_CLOSE_ON_FREE);
    if (ev_buf == NULL) {
        close(*file_fd);
        free(file_fd);
        return;
    }

    bufferevent_setcb(ev_buf,
                      socket_read_cb,     /* callback чтения*/
                      NULL,               /* callback записи*/
                      error_callback,     /* callback ошибки*/
                      file_fd);           /* передаем дескриптор файла в callback-функции */

    /* устанавливаем таймаут */
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    bufferevent_set_timeouts(ev_buf, &timeout, NULL);

    /* Активируем событие */
    ret = bufferevent_enable(ev_buf, EV_READ);
    if (ret < 0) {
        bufferevent_free(ev_buf);
        close(*file_fd);
        free(file_fd);
        return;
    }
}

int main(int argc, const char* argv[])
{
    uint16_t port;
    DIR *dir;
    struct sockaddr_in server;

    struct evconnlistener *ev_listener;
    struct event_base *ev_base;

    if(argc < 2) {
        fprintf(stdout, "Usage: server port path_to_files\n");
        fprintf(stdout, "\tport - port to listen\n");
        fprintf(stdout, "\tpath_to_files - path to save accepted files\n");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);

    dir = opendir(argv[2]);
    if (dir) {
        /* существует */
        closedir(dir);
    }
    else if (ENOENT == errno) {
        /* не существует, создаем */
        mkdir(argv[2], 0700);
    }
    else {
        perror("opendir:");
        exit(EXIT_FAILURE);
    }

    event_init();

    ev_base = event_base_new();
    if (!ev_base) {
        exit(EXIT_FAILURE);
    }

    /* Подготавливаем сокет */
    memset((void *) &server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    ev_listener = evconnlistener_new_bind(ev_base,
                                          accept_connection_cb, /* будет вызвана при попытки подключения*/
                                          (void *)argv[2],                 /* можно передать данные в callback-функцию */
                                          (LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE),
                                          -1,
                                          (struct sockaddr *) &server,
                                          sizeof(server));
    if(!ev_listener) {
        exit(EXIT_FAILURE);
    }

    return event_base_dispatch(ev_base);
}

