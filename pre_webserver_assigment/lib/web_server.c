#include<sys/socket.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<string.h>
#include<unistd.h>

#define CONN_QUEUE 5

int cleanup(int serv_sock_fd, FILE *debg_ofp)
{
  if (serv_sock_fd >= 0) {
    close(serv_sock_fd);
  }
  if (debg_ofp != NULL) {
    fclose(debg_ofp);
  }
  return 0;
}
			
int bootstrap_server(int *serv_sock_fd, int port_no, FILE *debg_ofp)
{
  int                status;
  struct sockaddr_in serv_addr;
  
  *serv_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (*serv_sock_fd < 0) {
    fprintf(debg_ofp, "Error opening socket\n");
    fprintf(stderr, "Error opening socket\n");
    return 1;
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port_no);
  status = bind(*serv_sock_fd,(struct sockaddr *) &serv_addr,
		sizeof(serv_addr));
  if (status < 0) {
    fprintf(debg_ofp, "Error binding port no: %d\n", port_no);
    return 1;
  }
  return 0;
}
  
int accept_connection(int serv_sock_fd, FILE *debg_ofp)
{
  struct sockaddr_in    cli_addr;
  int                   status;
  int                   cli_len;
  int                   new_sock_conn;
  char                  buf[8192];

  listen(serv_sock_fd, CONN_QUEUE);
  cli_len = sizeof(cli_addr);
  new_sock_conn = accept(serv_sock_fd,
			 (struct sockaddr *) &cli_addr,(socklen_t *) &cli_len);
  if (new_sock_conn < 0) {
    fprintf(debg_ofp, "ERROR: in accepting connection\n");
    return 1;
  }
  bzero(buf, 8192);
  status = read(new_sock_conn, buf, 8192);
  if (status < 0) {
    fprintf(debg_ofp, "ERROR: in reading from socket\n");
    return 1;
  }
  fprintf(debg_ofp, "INFO: Incoming message:\n%s\n",buf);
  parse_http_request(buf);
  close(new_sock_conn);
  return 0;
}

int parse_http_request(char *req)
{
  
}
  


  
