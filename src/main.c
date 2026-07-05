#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#include "include/itypes.h"
#include "include/utils.h"

#define PORT 8080

static void* handle_request(void *void_fd)
{
    i32 fd = *(i32 *)void_fd;
    free(void_fd);
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

    printf("server running on port %d\n", PORT);

    pthread_attr_t attr = {0};
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    u32 stack_size = KB(256);
    if (stack_size < PTHREAD_STACK_MIN) stack_size = PTHREAD_STACK_MIN;
    pthread_attr_setstacksize(&attr, stack_size);

    printf("%d\n", PTHREAD_STACK_MIN);

    // for (;;) {
    //     struct sockaddr_in client_addr = {0};
    //     socklen_t addrlen = sizeof(client_addr);

    //     i32 client_fd = accept(serv_fd, (struct sockaddr*)&client_addr, &addrlen);
    //     if (client_fd == -1) {
    //         perror("Accept failed");
    //         continue;
    //     }

    //     i32 *exclusive_fd = malloc(sizeof(i32));
    //     assert(exclusive_fd != NULL);
    //     *exclusive_fd = client_fd;
        
    //     pthread_t tid;

    //     if (pthread_create(&tid, &attr, handle_request, exclusive_fd) != 0) {
    //         perror("Failed to create thread");
    //         close(client_fd);
    //         free(exclusive_fd);
    //     }

    // }

    pthread_attr_destroy(&attr);
    shutdown(serv_fd, SHUT_RD);
    close(serv_fd);
    return 0;
}
