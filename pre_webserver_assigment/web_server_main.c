#include "web_server.h"
#include <string.h>
#include <signal.h>

FILE *debg_ofp;
int web_serv_sock_fd;
static volatile int keep_running = 1;
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

void int_handler(int sig)
{
    cleanup(web_serv_sock_fd, debg_ofp);
    exit(0);
}

int main(int argc, char *argv[])
{
    char               file_name[] = "web_server_trace.log";
    int                new_sock_conn;
    int                port_no;
    int                status;
    struct sockaddr    cli_addr;
    int                cli_len;
    
    signal(SIGINT, int_handler);
    signal(SIGPIPE, SIG_IGN);
    debg_ofp = fopen(file_name, "w");

    if (argc < 2) {
	fprintf(debg_ofp, "No port no entered hence exiting\n");
	fprintf(stderr, "Very Few Arguments enter port no\n");
	fflush(debg_ofp);
	cleanup(-1, debg_ofp);
	exit(1);
    }
    port_no = atoi(argv[1]);
    status = bootstrap_server(&web_serv_sock_fd, port_no, debg_ofp);
    if (status != 0){
	fprintf(debg_ofp, "ERROR in binding server port\n");
	fprintf(stderr, "ERROR bootstrapping server see log files for details\n");
	fflush(debg_ofp);
	cleanup(web_serv_sock_fd, debg_ofp);
	exit(status);
    }
    while(keep_running)
    {
	listen(web_serv_sock_fd, CONN_QUEUE);
	fprintf(debg_ofp, "\nNew TCP Connection\n");
	cli_len = sizeof(cli_addr);
	new_sock_conn = accept(web_serv_sock_fd, (struct sockaddr *) &cli_addr,
			       (socklen_t *) &cli_len);
	fprintf(debg_ofp, "DEBUG:  Connection accepted : %d\n", new_sock_conn);
        
	if (new_sock_conn < 0) {
	    fprintf(debg_ofp, "ERROR: in accepting connection\n");
	    fflush(debg_ofp);
	    cleanup(web_serv_sock_fd, debg_ofp);
	    return 1;
	}
	status = handle_connection(new_sock_conn, cli_addr, cli_len, debg_ofp);
	if (status != 0){
	    fprintf(debg_ofp, "ERROR in accepting connection\n");
	    fprintf(stderr,
		    "ERROR in accepting connection see log files for details\n");
	    fflush(debg_ofp);
	    continue;
	}
    }
    cleanup(web_serv_sock_fd, debg_ofp);
}
