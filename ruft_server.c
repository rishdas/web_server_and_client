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
ruft_server_traff_info_t *traff_info;
unsigned int max_wd;

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
    ctx->payload = (char *)malloc(ctx->payload_length+1);
    bzero(ctx->payload, ctx->payload_length);
    if (ctx->payload_length != 0) {
	strncpy(ctx->payload, pkt.payload, ctx->payload_length);
    } else {
	ctx->payload[0] = '\0';
    }
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
    if(ctx.payload != NULL) {
	strncpy(pkt->payload, ctx.payload, ctx.payload_length);
    } else {
	pkt->payload[0] = '\0';
    }
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
    sscanf(ctx.payload, "%s %s", method, rqst_info->file_name);
    fprintf(debg_ofp, "Method : %s File_name: %s\n",
	    method, rqst_info->file_name);
    method[strlen("GET")+1] = '\0';
    rqst_info->file_name[strlen(rqst_info->file_name)+1] = '\0';
    if (strncmp(method, "GET", strlen("GET")) != 0) {
	/*Error Request Format*/
	fprintf(debg_ofp, "Bad Request\n");
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
int ruft_server_add_traff_info(ruft_pkt_ctx_t ctx, unsigned int index,
			       FILE *debg_ofp)
{
    traff_info[index].seg_no = index;
    traff_info[index].no_ack_recvd = 0;
    traff_info[index].is_acked = FALSE;
    traff_info[index].first_byte = ctx.seq_no;
    traff_info[index].last_byte = ctx.payload_length + ctx.seq_no -1;
    return 0;
}
int ruft_server_set_sent_time(unsigned int index)
{
    gettimeofday(&(traff_info[index].sent_time), 0);
    return 0;
}
int ruft_server_set_ack_recv_time(unsigned int index)
{
    gettimeofday(&(traff_info[index].ack_recv_time), 0);
    traff_info[index].rtt = (traff_info[index].ack_recv_time.tv_sec
			     -traff_info[index].sent_time.tv_sec)*1000000
	+ traff_info[index].ack_recv_time.tv_usec
	- traff_info[index].sent_time.tv_usec;
    return 0;
}
int ruft_server_set_ack_recv(unsigned int index)
{
    traff_info[index].no_ack_recvd = traff_info[index].no_ack_recvd + 1;
    if (traff_info[index].no_ack_recvd < 2) {
	ruft_server_set_ack_recv_time(index);
    }
    return 0;
    
}
int ruft_server_set_max_wd(ruft_server_rqst_info_t rqst_info, FILE *debg_ofp)
{
    FILE        *uri_file_p;
    struct stat f_stat;
    int         uri_file_fd;
    int         no_of_segments;
    int         remainder = 0;
    
    uri_file_p = fopen(rqst_info.file_name, "r");
    uri_file_fd = fileno(uri_file_p);
    fstat(uri_file_fd, &f_stat);
    no_of_segments = f_stat.st_size/MAX_PAYLOAD;
    if (f_stat.st_size % MAX_PAYLOAD != 0) {
	no_of_segments = no_of_segments + 1;
	remainder = f_stat.st_size % MAX_PAYLOAD;
    }
    max_wd = no_of_segments;
    fclose(uri_file_p);
    return 0;
 
}
int ruft_server_create_traff_window(ruft_server_rqst_info_t rqst_info,
				    FILE *debg_ofp)
{
    switch(server_state)
    {
    case SV_REPLY_ERR:
	max_wd = 1;
	break;
    case SV_SLOW_START:
	ruft_server_set_max_wd(rqst_info, debg_ofp);
	break;
    default:
	break;
    }
    traff_info = (ruft_server_traff_info_t *)
	malloc(max_wd*sizeof(ruft_server_traff_info_t));
    bzero(traff_info, max_wd*sizeof(traff_info));
    return 0;
}
int ruft_server_send_file_seg(ruft_pkt_ctx_t req_ctx,
			      ruft_server_rqst_info_t rqst_info, int is_last_pkt,
			      unsigned int index, FILE *debg_ofp)
{
    ruft_pkt_ctx_t  reply_ctx;
    int             status = 0;
    FILE            *uri_file_p;
    int             last_byte = MAX_PAYLOAD;

    uri_file_p = fopen(rqst_info.file_name, "r");
    if (is_last_pkt != TRUE) {
	fseek(uri_file_p, index*MAX_PAYLOAD, SEEK_SET);
	last_byte = MAX_PAYLOAD;
    } else {
	fseek(uri_file_p, 0, SEEK_END);
	last_byte = ftell(uri_file_p);
	fseek(uri_file_p, index*MAX_PAYLOAD, SEEK_SET);
	last_byte = last_byte - index*MAX_PAYLOAD;
    }
    reply_ctx.is_ack = TRUE;
    reply_ctx.is_data_pkt = TRUE;
    reply_ctx.is_last_pkt = is_last_pkt;
    
    reply_ctx.ack_no = req_ctx.seq_no+req_ctx.payload_length;
    reply_ctx.seq_no = req_ctx.ack_no;
    reply_ctx.awnd = MAX_PAYLOAD; //TODO
    reply_ctx.payload_length = last_byte; //TODO
    reply_ctx.payload = (char *)malloc(reply_ctx.payload_length);
    bzero(reply_ctx.payload, reply_ctx.payload_length);
    fread(reply_ctx.payload, 1, last_byte, uri_file_p);
    ruft_server_add_traff_info(reply_ctx, index, debg_ofp);
    ruft_server_set_sent_time(index);
    status = ruft_server_send_pkt(reply_ctx, rqst_info, debg_ofp);
    return status;
}
int ruft_server_send_file(ruft_pkt_ctx_t req_ctx,
			  ruft_server_rqst_info_t rqst_info, FILE *debg_ofp)
{
    ruft_pkt_ctx_t ctx;
    unsigned int   i = 0;
    int            is_last_pkt = FALSE;
    int            status=0;
    server_state = SV_SLOW_START;
    ruft_server_create_traff_window(rqst_info, debg_ofp);
    ctx = req_ctx;

    if (i == max_wd-1) {
	is_last_pkt = TRUE;
    }
    
    while(server_state != SV_FILE_SENT)
    {
	ruft_server_send_file_seg(ctx, rqst_info, is_last_pkt, i, debg_ofp);
	bzero(&ctx, sizeof(ctx));
	status = ruft_server_recv_pkt(&ctx, &rqst_info, debg_ofp);
	if (status != 0) {
	    return status;
	}
	if (ctx.is_ack == TRUE && ctx.is_data_pkt == FALSE
	    && ctx.is_last_pkt == FALSE){
	    ruft_server_set_ack_recv(i);
	    fprintf(stdout, "\nRTT: %lu\n", traff_info[i].rtt);
	}
	if (ctx.is_ack == TRUE && ctx.is_data_pkt == FALSE
	    && ctx.is_last_pkt == TRUE){
	    ruft_server_set_ack_recv(i);
	    fprintf(stdout, "\nRTT: %lu\n", traff_info[i].rtt);
	    server_state = SV_FILE_SENT;
	}
	i++;
	if (i == max_wd-1) {
	    is_last_pkt = TRUE;
	}
	
    }
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
    reply_ctx.payload_length = strlen("Arya Error: File Not Found")+1;
    reply_ctx.payload = (char *)malloc(reply_ctx.payload_length);
    bzero(reply_ctx.payload, reply_ctx.payload_length);
    strncpy(reply_ctx.payload, "Arya Error: File Not Found",
	    reply_ctx.payload_length-1);
    reply_ctx.payload[reply_ctx.payload_length-1] = '\0';
    ruft_server_add_traff_info(reply_ctx, 0, debg_ofp);
    ruft_server_set_sent_time(0);
    /*Set the time for Zeroth segment as this is error scenario*/
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
    ruft_server_create_traff_window(rqst_info, debg_ofp);
    status = ruft_server_send_err_msg(req_ctx, rqst_info, debg_ofp);
    if (status != 0) {
	return status;
    }
    
    status = ruft_server_recv_pkt(&ctx, &recv_rqst_info, debg_ofp);
    if (status != 0) {
	return status;
    }
    if (ctx.is_ack == TRUE && ctx.is_last_pkt == TRUE
	&& ctx.is_data_pkt == FALSE){
	ruft_server_set_ack_recv(0);
	fprintf(stdout, "\nRTT: %lu\n", traff_info[0].rtt);
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
	ruft_server_handle_err(ctx, rqst_info, debg_ofp);
	break;
    }
    fprintf(stdout, "\nCounter\n");
    fflush(stdout);
    server_state = SV_WAIT;
    /* if (traff_info != NULL) { */
    /* 	free(traff_info); */
    /* } */
    /* traff_info = NULL; */
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
