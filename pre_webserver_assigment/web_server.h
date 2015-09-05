#include<stdio.h>
#include<stdlib.h>


int bootstrap_server(int *serv_sock_fd, int port_no, FILE *debg_ofp);
int cleanup(int serv_sock_fd, FILE *debg_ofp);
int accept_connection(int serv_sock_fd,FILE *debg_ofp);
int parse_http_request(char *req);
