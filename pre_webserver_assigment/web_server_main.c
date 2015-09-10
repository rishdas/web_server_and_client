#include "web_server.h"
#include <string.h>
#include <signal.h>

FILE *debg_ofp;
int web_serv_sock_fd;
static volatile int keep_running = 1;
void int_handler(int sig)
{
    cleanup(web_serv_sock_fd, debg_ofp);
    exit(0);
    /* char    c; */
    /* signal(sig, SIG_IGN); */
    /* printf("\nDo you want to stop the Stark Server? [y/n]"); */
    /* c = getchar(); */
    /* if (c == 'Y' || c == 'y') { */
    /* 	keep_running = 0; */
    /* } else { */
    /* 	signal(SIGINT, int_handler); */
    /* } */
}
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
    char               *file_name;
    int                new_sock_conn;
    int                port_no;
    int                status;
    struct sockaddr_in cli_addr;
    int                cli_len;
    
    signal(SIGINT, int_handler);
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
    while(keep_running)
    {
	listen(web_serv_sock_fd, CONN_QUEUE);
	cli_len = sizeof(cli_addr);
	new_sock_conn = accept(web_serv_sock_fd,(struct sockaddr *) &cli_addr,
			       (socklen_t *) &cli_len);
	if (new_sock_conn < 0) {
	    fprintf(debg_ofp, "ERROR: in accepting connection\n");
	    return 1;
	}
	status = handle_connection(new_sock_conn, cli_addr, cli_len, debg_ofp);
	if (status != 0){
	    fprintf(debg_ofp, "ERROR in accepting connection\n");
	    fprintf(stderr,
		    "ERROR in accepting connection see log files for details\n");
	    continue;
	}
    }
    cleanup(web_serv_sock_fd, debg_ofp);
}
