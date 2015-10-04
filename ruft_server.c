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

FILE *debg_ofp;
int udp_serv_sock_fd;
ruft_server_states_t server_state;

/*
 * Free the return value after use
 */
typedef struct ruft_server_rqst_info_
{
    struct sockaddr_in cli_addr;
    size_t cli_len;
    char file_name[MAXLINE];
} ruft_server_rqst_info_t;
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

int ruft_server_bootstrap(int *serv_sock_fd, int port_no, FILE *debg_ofp)
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
int ruft_server_pkt_info_to_ctx(ruft_pkt_info_t pkt, ruft_pkt_ctx_t *ctx,
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

int ruft_server_pkt_ctx_to_info(ruft_pkt_info_t *pkt, ruft_pkt_ctx_t ctx,
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
int ruft_server_print_pkt_ctx(ruft_pkt_ctx_t ctx, FILE *debg_ofp)
{
    fprintf(stdout, "Message :\nAck : %s \n"\
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

int ruft_server_free_ctx(ruft_pkt_ctx_t ctx)
{
    free(ctx.payload);
    return 0;
}
int ruft_server_send_pkt(ruft_pkt_ctx_t ctx, ruft_server_rqst_info_t req_info,
			 FILE *debg_ofp)
{
    ruft_pkt_info_t pkt;
    int             num_bytes;

    bzero(&pkt, sizeof(pkt));
    ruft_server_pkt_ctx_to_info(&pkt, ctx, debg_ofp);
    num_bytes = sendto(udp_serv_sock_fd, &pkt, sizeof(pkt), 0,
		       (struct sockaddr *) &(req_info.cli_addr),
		       (req_info.cli_len));
    if (num_bytes == 0) {
	fprintf(debg_ofp,
		"UDP client connection closed at client side\n");
	return 1;
    }
	
    if (num_bytes < 0) {
	fprintf(debg_ofp, "ERROR: in UDP connection\n");
	fflush(debg_ofp);
	return 1;
    }
    return 0;
}
int ruft_server_recv_pkt(ruft_pkt_ctx_t *ctx, ruft_server_rqst_info_t *req_info,
			 FILE *debg_ofp)
{
    ruft_pkt_info_t pkt;
    int             num_bytes;

    bzero(&pkt, sizeof(pkt));

    num_bytes = recvfrom(udp_serv_sock_fd, &pkt, sizeof(pkt), 0,
			 (struct sockaddr *) &(req_info->cli_addr),
			 (socklen_t *)&(req_info->cli_len));
    if (num_bytes == 0) {
	fprintf(debg_ofp,
		"UDP client connection closed at client side\n");
	return 1;
    }
	
    if (num_bytes < 0) {
	fprintf(debg_ofp, "ERROR: in UDP connection\n");
	fflush(debg_ofp);
	return 1;
    }
    ruft_server_pkt_info_to_ctx(pkt, ctx, debg_ofp);
    ruft_server_print_pkt_ctx(*ctx, debg_ofp);
    return 0;
}
int ruft_server_process_req(ruft_pkt_ctx_t ctx,
			    ruft_server_rqst_info_t *rqst_info, FILE *debg_ofp)
{
    char method[10];
    FILE *uri_file_p;
    server_state = SV_PROC_REQ;
    sscanf("%s %s", method, rqst_info->file_name);
    fprintf(debg_ofp, "Method : %s File_name: %s", method, rqst_info->file_name);
    if (strncmp(method, "GET", strlen("GET")) != 0) {
	/*Error Request Format*/
	return 2;
    }
    uri_file_p = fopen(rqst_info->file_name, "r");
    if (uri_file_p == NULL) {
	fprintf(debg_ofp, "ERROR opening file file not found\n");
	return 2;
    }
    fclose(uri_file_p);
    return 1;
}

int ruft_server_send_file(ruft_pkt_ctx_t req_ctx,
			  ruft_server_rqst_info_t rqst_info, FILE *debg_ofp)
{
    return 0;
}

int ruft_server_send_err_msg(ruft_pkt_ctx_t req_ctx,
			     ruft_server_rqst_info_t rqst_info, FILE *debg_ofp)
{
    ruft_pkt_ctx_t  reply_ctx;
    int             status = 0;

    reply_ctx.is_ack = TRUE;
    reply_ctx.is_data_pkt = FALSE;
    reply_ctx.is_last_pkt = TRUE;
    
    reply_ctx.ack_no = req_ctx.seq_no+req_ctx.payload_length;
    reply_ctx.seq_no = req_ctx.ack_no;
    reply_ctx.awnd = MAX_PAYLOAD; //TODO
    reply_ctx.payload_length = strlen("Arya Error: File Not Found\0");
    reply_ctx.payload = (char *)malloc(reply_ctx.payload_length);
    strncpy(reply_ctx.payload, "Arya Error: File Not Found\0",
	    reply_ctx.payload_length);
    status = ruft_server_send_pkt(reply_ctx, rqst_info, debg_ofp);
    return status;
    
}
int ruft_server_handle_err(ruft_pkt_ctx_t req_ctx,
			   ruft_server_rqst_info_t rqst_info, FILE *debg_ofp)
{
    ruft_pkt_ctx_t          ctx;
    ruft_server_rqst_info_t recv_rqst_info;
    int                     status;
    bzero(&ctx, sizeof(ctx));
    server_state = SV_REPLY_ERR;
    status = ruft_server_send_err_msg(ctx, rqst_info, debg_ofp);
    if (status != 0) {
	return status;
    }
    
    status = ruft_server_recv_pkt(&ctx, &recv_rqst_info, debg_ofp);
    if (status != 0) {
	return status;
    }
    return status;
}
int ruft_server_handle_pkt(ruft_pkt_info_t pkt,struct sockaddr_in cli_addr,
			   int cli_len,FILE *debg_ofp)
{
    ruft_pkt_ctx_t          ctx;
    int                     status;
    ruft_server_rqst_info_t rqst_info;

    bzero(&ctx, sizeof(ctx));
    bzero(&rqst_info, sizeof(rqst_info));


    rqst_info.cli_addr = cli_addr;
    rqst_info.cli_len  = cli_len;
    
    ruft_server_pkt_info_to_ctx(pkt, &ctx, debg_ofp);
    fprintf(stdout, "Request Recieved\n");
    ruft_server_print_pkt_ctx(ctx, debg_ofp);
    
    status = ruft_server_process_req(ctx, &rqst_info, debg_ofp);
    switch(status)
    {
    case 1:
	ruft_server_send_file(ctx, rqst_info, debg_ofp);
	break;
    case 2:
	ruft_server_handle_err(ctx, rqst_info, debg_ofp);
	break;
    default:
	break;
    }
    server_state = SV_WAIT;
    return 0;
}

int main(int argc, char *argv[])
{
    char                  file_name[] = "udp_server_trace.log";
    int                   num_bytes;
    int                   port_no;
    int                   status;
    struct sockaddr_in    cli_addr;
    size_t                cli_len;
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
    status = ruft_server_bootstrap(&udp_serv_sock_fd, port_no, debg_ofp);
    if (status != 0){
	fprintf(debg_ofp, "ERROR in binding server port\n");
	fprintf(stderr, "ERROR bootstrapping server see log files for details\n");
	fflush(debg_ofp);
	cleanup(udp_serv_sock_fd, debg_ofp);
	exit(status);
    }
    server_state = SV_WAIT;
    while(server_state == SV_WAIT)
    {
	cli_len = sizeof(cli_addr);
	num_bytes = recvfrom(udp_serv_sock_fd, &pkt, sizeof(pkt), 0,
			     (struct sockaddr *) &cli_addr,
			     (socklen_t *) &cli_len);
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
	ruft_server_handle_pkt(pkt, cli_addr, cli_len, debg_ofp);
	
    }
    cleanup(udp_serv_sock_fd, debg_ofp);
}
