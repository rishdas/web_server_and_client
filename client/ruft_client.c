#include<stdio.h>
#include<stdlib.h>
#include<netdb.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string.h>
#include<signal.h>
#include<sys/time.h>
#include "ruft.h"



FILE *debg_ofp;
FILE     *ifp;
int  client_sock_fd;
#define MAX_BUFFER 1000
typedef struct client_info_
{
    int            port;
    char           *file_name;
    struct hostent *server_addr;
    int            is_filename_in_disk;
} client_info_t;
    
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

int parse_cmd_line_args(int argc, char *argv[],
			client_info_t *client_info, FILE *debg_ofp)
{   
    if(argc< 4) {
	fprintf(debg_ofp, "ERROR: Very few arguments "\
		"the format for is \n"\
		"client\t<server_host>\t<server_port>" \
		"\t<connection_type>\t<filename.txt>\n");
	return -1;
	
    }
    bzero(client_info, sizeof(client_info));
    client_info->server_addr     = gethostbyname(argv[1]);
    client_info->port            = atoi(argv[2]);
    client_info->file_name       = argv[3];
    
    fprintf(debg_ofp, "Parsed command line arguments\n");
    fprintf(debg_ofp, "Server Address: %s\t",
	    client_info->server_addr->h_name);
    fprintf(debg_ofp, "Port No: %d\t", client_info->port);
    fprintf(debg_ofp, "File Name: %s\t\n", client_info->file_name);
    
    return 0;
    
}

int bootstrap_client(int *client_sock_fd, client_info_t client_info,
		     struct sockaddr_in *server_addr, FILE *debg_ofp)
{
    int                status;

    *client_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*client_sock_fd < 0) {
	fprintf(debg_ofp, "Error opening socket\n");
	fprintf(stderr, "Error opening socket\n");
	return 1;
    }
    bzero((char *) server_addr, sizeof(server_addr));
    server_addr->sin_family = AF_INET;
    bcopy((char *)client_info.server_addr->h_addr,
	  (char *)&server_addr->sin_addr.s_addr,
	  client_info.server_addr->h_length);
    server_addr->sin_port = htons(client_info.port);
    return 0;
}

int main(int argc, char *argv[])
{

    int                status;
    client_info_t      client_info;
    char               file_name[] = "udp_client_trace.log\0";
    struct sockaddr_in server_addr;
    ruft_pkt_info_t    pkt;
    
    bzero(&pkt, sizeof(pkt));
    signal(SIGPIPE, SIG_IGN);
    debg_ofp = fopen(file_name, "w");
    status = parse_cmd_line_args(argc, argv, &client_info, debg_ofp);
    if(status != 0) {
	fprintf(stderr, "ERROR: Invalid arguments check log\n");
	fclose(debg_ofp);
	exit(1);
    }
    status = bootstrap_client(&client_sock_fd, client_info,
			      &server_addr, debg_ofp);
    if(status != 0) {
	fprintf(stderr, "ERROR: Creating a socket\n");
	fprintf(debg_ofp, "ERROR: Creating a socket\n");
	fclose(debg_ofp);
	exit(1);
    }
    pkt.flags = htons(1);
    pkt.awnd = htons(2);
    pkt.ack_no = htonl(3);
    pkt.seq_no = htonl(4);
    pkt.payload_length = htonl(10);
    strncpy(pkt.payload, file_name, strlen(file_name)+1);
    fprintf(stdout, "Recieved Message :\nFlags : %d \n"\
	    "Advertised Window : %d \nAck No: %d \nSeq No: %d\n"\
	    "Payload Length: %d \nPayload: %s\n", ntohs(pkt.flags),
	    ntohs(pkt.awnd), ntohl(pkt.ack_no), ntohl(pkt.seq_no),
	    ntohl(pkt.payload_length), pkt.payload);

    status = sendto(client_sock_fd, &(pkt), sizeof(pkt), 0,
		    (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (status < 0) {
	fprintf(stderr, "Error writing into the socket\n");
	fprintf(debg_ofp, "Error writing into the socket\n");
	close(client_sock_fd);
	return -1;
    }
    fprintf(debg_ofp, "%s", file_name);
    fclose(debg_ofp);
    return 0;
}



