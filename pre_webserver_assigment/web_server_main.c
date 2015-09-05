#include "web_server.h"
#include <string.h>

FILE *debg_ofp;
char *get_debug_file_name()
{
  char   *file_name = malloc(50*sizeof(char));
  file_name = strncat(file_name, "web_server_", 50);
  file_name = strncat(file_name, __DATE__, 50);
  file_name = strncat(file_name, __TIME__, 50);
  file_name = strncat(file_name, ".log", 30);
  return file_name;
}
int main(int argc, char *argv[])
{
  char   *file_name;
  int    web_serv_sock_fd, conn_sock_fd;
  int    port_no;
  int    status;
  file_name = get_debug_file_name();
  debg_ofp = fopen(file_name, "w");
  free(file_name);
  if (argc < 2) {
    fprintf(debg_ofp, "No port no entered hence exiting\n");
    fprintf(stderr, "Very Few Arguments enter port no\n");
    cleanup(-1, debg_ofp);
    exit(1);
  }
  port_no = atoi(argv[1]);
  status = bootstrap_server(&web_serv_sock_fd, port_no, debg_ofp);
  if (status != 0){
    fprintf(debg_ofp, "ERROR in binding server port\n");
    fprintf(stderr, "ERROR bootstrapping server see log files for details\n");
    cleanup(web_serv_sock_fd, debg_ofp);
    exit(status);
  }
  status = accept_connection(web_serv_sock_fd, debg_ofp);
  if (status != 0){
    fprintf(debg_ofp, "ERROR in accepting connection\n");
    fprintf(stderr, "ERROR in accepting connection see log files for details\n");
    cleanup(web_serv_sock_fd, debg_ofp);
    exit(status);
  }
  cleanup(web_serv_sock_fd, debg_ofp);
}
