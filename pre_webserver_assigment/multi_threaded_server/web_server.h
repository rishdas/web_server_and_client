#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string.h>
#include<sys/stat.h>

#define CONN_QUEUE 5

typedef struct hdl_conn_args_
{
    int             new_sock_conn;
    struct sockaddr cli_addr;
    int             cli_len;
    FILE            *debg_ofp;
    int             status;
} hdl_conn_args_t;

int bootstrap_server(int *serv_sock_fd, int port_no, FILE *debg_ofp);
int cleanup(int serv_sock_fd, FILE *debg_ofp);
void *handle_connection(void *thrd_args);
