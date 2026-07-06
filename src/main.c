#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "include/itypes.h"
#include "include/utils.h"

#define PORT 8080
#define VERSION "HTTP/1.1"

#define STATUS_LINE_MAX_LEN 64
#define HEADER_FIELD_MAX_LEN 256

typedef enum {
    HTTP_OK                    = 200,
    HTTP_CONTINUE              = 100,
    HTTP_NOT_MODIFIED          = 304,
    HTTP_BAD_REQUEST           = 400,
    HTTP_NOT_FOUND             = 404,
    HTTP_METHOD_NOT_ALLOWED    = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_NOT_IMPLEMENTED       = 501,
    HTTP_VERSION_NOT_SUPPORTED = 505
} HTTPStatusCode;

static inline const char* http_status_reason_phrase(HTTPStatusCode code)
{
    switch (code) {
        case HTTP_OK: return "Ok";
        case HTTP_CONTINUE: return "Continue";
        case HTTP_NOT_MODIFIED: return "Not Modified";
        case HTTP_BAD_REQUEST: return "Bad Request";
        case HTTP_NOT_FOUND: return "Not Found";
        case HTTP_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTP_NOT_IMPLEMENTED: return "Not Implemented";
        case HTTP_VERSION_NOT_SUPPORTED: return "Version Not Supported";
        default: return "Unknown";
    }
}

static inline u32 write_status_line(HTTPStatusCode code, char* dest)
{
    return snprintf(dest, STATUS_LINE_MAX_LEN, "%s %u %s\r\n", VERSION, code, http_status_reason_phrase(code));
    
}

static inline u32 write_http_date(char *dest) {
    time_t raw_time;
    struct tm gmt_time;
    time(&raw_time);
    gmtime_r(&raw_time, &gmt_time); 
    return strftime(dest, 32, "%a, %d %b %Y %H:%M:%S GMT", &gmt_time);
}

static inline u32 write_header(char* dest, usize len)
{
    char date[32];
    write_http_date(date);
    return snprintf(dest, 512,
        "Date: %s\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
    , date, len);
}

static void* request_handler(void *void_fd)
{
    i32 fd = *(i32 *)void_fd;
    free(void_fd);

    char buffer[2048];
    
    ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';


        char status_line[STATUS_LINE_MAX_LEN];
        char header[HEADER_FIELD_MAX_LEN];
        const char* body =
            "<html>"
                "<body>"
                    "<h1>Iamhari.</h1>"
                "</body>"
            "</html>";

        write_status_line(HTTP_OK, status_line);
        write_header(header, strlen(body));

        snprintf(buffer, 2048, "%s%s\r\n%s", status_line, header, body);

        send(fd, buffer, strlen(buffer), 0);
    }
    close(fd);
    return NULL;
}

i32 main()
{
    i32 serv_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(serv_fd != -1);

    i32 opt = 1;
    setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);


    assert(bind(serv_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != -1);
    assert(listen(serv_fd, SOMAXCONN) != -1);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, KB(256));

    printf("http server running on port %d\n", PORT);

    for (;;) {
        struct sockaddr_in client_addr = {0};
        socklen_t addrlen = sizeof(client_addr);

        i32 client_fd = accept(serv_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }

        i32 *exclusive_fd = malloc(sizeof(i32));
        assert(exclusive_fd != NULL);
        *exclusive_fd = client_fd;
        
        pthread_t tid;

        if (pthread_create(&tid, &attr, request_handler, exclusive_fd) != 0) {
            perror("Failed to create thread");
            close(client_fd);
            free(exclusive_fd);
        }

    }

    pthread_attr_destroy(&attr);
    shutdown(serv_fd, SHUT_RD);
    close(serv_fd);
    return 0;
}
