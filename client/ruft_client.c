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
ruft_client_states_t client_state;

typedef struct client_info_
{
    int            port;
    char           *file_name;
    struct hostent *server_addr;
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

int ruft_client_bootstrap(int *client_sock_fd, client_info_t client_info,
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

int ruft_client_pkt_info_to_ctx(ruft_pkt_info_t pkt, ruft_pkt_ctx_t *ctx,
				FILE *debg_ofp)
{
    short flags = 0;
    flags = ntohs(pkt.flags);
    /*Extract Flags*/
    if ((flags&0x10) == 0x10) {
	ctx->is_ack = TRUE;
    } else {
	ctx->is_ack = FALSE;
    }
    
    if ((flags&0x20) == 0x20) {
	ctx->is_data_pkt = TRUE;
    } else {
	ctx->is_data_pkt = FALSE;
    }
    if ((flags&0x40) == 0x40) {
	ctx->is_last_pkt = TRUE;
    } else {
	ctx->is_last_pkt = FALSE;
    }
    ctx->ack_no = ntohl(pkt.ack_no);
    ctx->seq_no = ntohl(pkt.seq_no);
    ctx->awnd = ntohs(pkt.awnd);
    ctx->payload_length = ntohl(pkt.payload_length);
    ctx->payload = (char *)malloc(ctx->payload_length);
    strncpy(ctx->payload, pkt.payload, ctx->payload_length);
    return 0;
}

int ruft_client_pkt_ctx_to_info(ruft_pkt_info_t *pkt, ruft_pkt_ctx_t ctx,
				FILE *debg_ofp)
{
    /*Fill the Flags*/
    short flags = 0;
    if (ctx.is_ack == TRUE) {
	flags = flags|0x10;
    } else {
	flags = flags|0x00;
    }

    if (ctx.is_data_pkt == TRUE) {
	flags = flags|0x20;
    } else {
	flags = flags|0x00;
    }

    if (ctx.is_last_pkt == TRUE) {
	flags = flags|0x40;
    } else {
	flags = flags|0x00;
    }
    pkt->flags = htons(flags);
    pkt->ack_no = htonl(ctx.ack_no);
    pkt->seq_no = htonl(ctx.seq_no);
    pkt->awnd = htons(ctx.awnd);
    pkt->payload_length = htonl(ctx.payload_length);
    strncpy(pkt->payload, ctx.payload, ctx.payload_length);
    return 0;
}

int ruft_client_print_pkt_ctx(ruft_pkt_ctx_t ctx, FILE *debg_ofp)
{
    fprintf(stdout, "Recieved Message :\nAck : %s \n"\
	    "Data Pkt: %s\nLast Pkt: %s\n"\
	    "Advertised Window : %d \nAck No: %d \nSeq No: %d\n"\
	    "Payload Length: %d \nPayload: %s\n",
	    (ctx.is_ack == TRUE)?"TRUE":"FALSE",
	    (ctx.is_data_pkt == TRUE)?"TRUE":"FALSE",
	    (ctx.is_last_pkt == TRUE)?"TRUE":"FALSE",
	    ctx.awnd, ctx.ack_no, ctx.seq_no,
	    ctx.payload_length, ctx.payload);
    return 0;
}

int ruft_client_free_ctx(ruft_pkt_ctx_t ctx)
{
    free(ctx.payload);
    return 0;
}
int ruft_client_build_get_rqst(ruft_pkt_ctx_t *ctx, client_info_t client_info,
			       FILE *debg_ofp)
{
    ctx->is_ack = FALSE;
    ctx->is_data_pkt = FALSE;
    ctx->is_last_pkt = FALSE;
    ctx->awnd = 1;
    ctx->ack_no = 0;
    ctx->seq_no = 1;
    ctx->payload_length = strlen(client_info.file_name)+strlen("GET")+1;
    ctx->payload = (char *)malloc(ctx->payload_length);
    sprintf(ctx->payload, "GET %s", client_info.file_name);
    ruft_client_print_pkt_ctx(*ctx, debg_ofp);
    return 0;
}
int main(int argc, char *argv[])
{

    int                status;
    client_info_t      client_info;
    char               file_name[] = "udp_client_trace.log\0";
    struct sockaddr_in server_addr;
    ruft_pkt_info_t    pkt;
    ruft_pkt_ctx_t     ctx;
    
    bzero(&pkt, sizeof(pkt));
    bzero(&ctx, sizeof(ctx));
    signal(SIGPIPE, SIG_IGN);
    debg_ofp = fopen(file_name, "w");
    status = parse_cmd_line_args(argc, argv, &client_info, debg_ofp);
    if(status != 0) {
	fprintf(stderr, "ERROR: Invalid arguments check log\n");
	fclose(debg_ofp);
	exit(1);
    }
    status = ruft_client_bootstrap(&client_sock_fd, client_info,
				   &server_addr, debg_ofp);
    if(status != 0) {
	fprintf(stderr, "ERROR: Creating a socket\n");
	fprintf(debg_ofp, "ERROR: Creating a socket\n");
	fclose(debg_ofp);
	exit(1);
    }
    client_state = CL_SEND_REQUEST;
    ruft_client_build_get_rqst(&ctx, client_info, debg_ofp);
    ruft_client_pkt_ctx_to_info(&pkt, ctx, debg_ofp);

    status = sendto(client_sock_fd, &(pkt), sizeof(pkt), 0,
		    (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (status < 0) {
	fprintf(stderr, "Error writing into the socket\n");
	fprintf(debg_ofp, "Error writing into the socket\n");
	close(client_sock_fd);
	return -1;
    }
    bzero(&pkt, sizeof(pkt));
    bzero(&ctx, sizeof(ctx));
    status = recvfrom(client_sock_fd, &pkt, sizeof(pkt), 0, NULL, NULL);
    if (status < 0) {
	fprintf(stderr, "Error reading from the socket\n");
	fprintf(debg_ofp, "Error reading into the socket\n");
	close(client_sock_fd);
	return -1;
    }
    ruft_client_pkt_info_to_ctx(pkt, &ctx, debg_ofp);
    ruft_client_print_pkt_ctx(ctx, debg_ofp);
    
    fclose(debg_ofp);
    return 0;
}



