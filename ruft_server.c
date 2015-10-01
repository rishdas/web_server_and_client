#include<sys/socket.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/time.h>
#include<signal.h>
#include "ruft.h"


#define MAXLINE 20
#define WAIT    10
#define MAX_BUFFER 1200
#define MAX_PAYLOAD 1000

FILE *debg_ofp;
int udp_serv_sock_fd;
static volatile int keep_running = 1;

/*
 * Free the return value after use
 */

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
    cleanup(udp_serv_sock_fd, debg_ofp);
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

int bootstrap_server(int *serv_sock_fd, int port_no, FILE *debg_ofp)
{
    int                status;
    struct sockaddr_in serv_addr;
  
    *serv_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
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

int main(int argc, char *argv[])
{
    char                  file_name[] = "udp_server_trace.log";
    int                   num_bytes;
    int                   port_no;
    int                   status;
    struct sockaddr_in    cli_addr;
    int                   cli_len;
    char                  buf[MAX_BUFFER];
    ruft_pkt_info_t       pkt;
    
    signal(SIGINT, int_handler);
    ignore_sigpipe();
    debg_ofp = fopen(file_name, "w");
    bzero(&pkt, sizeof(pkt));

    if (argc < 2) {
	fprintf(debg_ofp, "No port no entered hence exiting\n");
	fprintf(stderr, "Very Few Arguments enter port no\n");
	fflush(debg_ofp);
	cleanup(-1, debg_ofp);
	exit(1);
    }
    port_no = atoi(argv[1]);
    status = bootstrap_server(&udp_serv_sock_fd, port_no, debg_ofp);
    if (status != 0){
	fprintf(debg_ofp, "ERROR in binding server port\n");
	fprintf(stderr, "ERROR bootstrapping server see log files for details\n");
	fflush(debg_ofp);
	cleanup(udp_serv_sock_fd, debg_ofp);
	exit(status);
    }
    while(keep_running)
    {
	cli_len = sizeof(cli_addr);
	num_bytes = recvfrom(udp_serv_sock_fd, &pkt, sizeof(pkt), 0,
			     (struct sockaddr *) &cli_addr, (socklen_t *) &cli_len);
	if (num_bytes == 0) {
	    fprintf(debg_ofp,
		    "UDP client connection closed at client side\n");
	    continue;
	}
	
	if (num_bytes < 0) {
	    fprintf(debg_ofp, "ERROR: in UDP connection\n");
	    fflush(debg_ofp);
	    cleanup(udp_serv_sock_fd, debg_ofp);
	    return 1;
	}
	fprintf(stdout, "Recieved Message :\nFlags : %d \n"\
		"Advertised Window : %d \nAck No: %d \nSeq No: %d\n"\
		"Payload Length: %d \nPayload: %s\n", ntohs(pkt.flags),
		ntohs(pkt.awnd), ntohl(pkt.ack_no), ntohl(pkt.seq_no),
		ntohl(pkt.payload_length), pkt.payload);
		
    }
    cleanup(udp_serv_sock_fd, debg_ofp);
}
