#include "web_server.h"
#include <string.h>
#include <signal.h>
#include <pthread.h>

FILE *debg_ofp;
int web_serv_sock_fd;
static volatile int keep_running = 1;
#define MAX_THREADS 100
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
void ignore_sigpipe(void)
{
    struct sigaction act;
    int status;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    act.sa_flags = SA_RESTART;
    status = sigaction(SIGPIPE, &act, NULL);
    if (status) {
        fprintf(stderr, "sigaction\n");
    }
}

int main(int argc, char *argv[])
{
    char               file_name[] = "web_server_trace.log";
    int                new_sock_conn;
    int                port_no;
    int                status;
    struct sockaddr    cli_addr;
    int                cli_len;
    pthread_t          pthread_arr[MAX_THREADS];
    int                i = 0, j = 0;
    hdl_conn_args_t    thrd_args[MAX_THREADS];
    void               *thrd_retval;
    
    signal(SIGINT, int_handler);
    ignore_sigpipe();
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
    i = 0;
    while(i<MAX_THREADS)
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
	thrd_args[i].new_sock_conn = new_sock_conn;
	thrd_args[i].cli_addr      = cli_addr;
	thrd_args[i].cli_len       = cli_len;
	thrd_args[i].debg_ofp      = debg_ofp;
	thrd_args[i].status        = 0;
	status = pthread_create(&pthread_arr[i], NULL,
				handle_connection, &thrd_args);
	i++;
    }
    cleanup(web_serv_sock_fd, debg_ofp);
}
