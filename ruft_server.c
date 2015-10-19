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

typedef struct ruft_server_stats_
{
    unsigned int t_rx;
    unsigned int t_tx;
    unsigned int ss_rx;
    unsigned int ss_tx;
    unsigned int ca_rx;
    unsigned int ca_tx;
} ruft_server_stats_t;
FILE *debg_ofp;
int udp_serv_sock_fd;
ruft_server_states_t server_state;
ruft_server_traff_info_t *traff_info;
unsigned int max_wd;
unsigned int cwnd = MAX_PAYLOAD;
unsigned int cwnd_seg = 1;
unsigned int rwnd = MAX_PAYLOAD;
unsigned int rwnd_seg = 1;
unsigned int ss_thresh = 50*MAX_PAYLOAD; //64kb
unsigned int ss_thresh_seg = 4;
unsigned long int est_rtt = 0;
unsigned long int dev_rtt = 0;
unsigned long int timeout = 500;
unsigned int no_dist_ack_recvd = 0;
ruft_server_stats_t serv_stats;
#define THRESHOLD 500

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
int ruft_server_add_rx_stat()
{
    switch(server_state)
    {
    case SV_SLOW_START:
	serv_stats.t_rx++;
	serv_stats.ss_rx++;
	break;
    case SV_CONG_AVOID:
	serv_stats.t_rx++;
	serv_stats.ca_rx++;
	break;
    default:
	serv_stats.t_rx++;
    }
    return 0;
}
int ruft_server_add_tx_stat()
{
    switch(server_state)
    {
    case SV_SLOW_START:
	serv_stats.t_tx++;
	serv_stats.ss_tx++;
	break;
    case SV_CONG_AVOID:
	serv_stats.t_tx++;
	serv_stats.ca_tx++;
	break;
    default:
	serv_stats.t_tx++;
    }
    return 0;
}
int ruft_server_print_stat(FILE *debg_ofp)
{
    fprintf(debg_ofp, "Total Packets Stats:\n");
    fprintf(debg_ofp, "TX: %u\t", serv_stats.t_tx);
    fprintf(debg_ofp, "RX: %u\n", serv_stats.t_rx);
    fprintf(debg_ofp, "Slow Start Packets Stats:\n");
    fprintf(debg_ofp, "TX: %u\t", serv_stats.ss_tx);
    fprintf(debg_ofp, "RX: %u\t", serv_stats.ss_rx);
    fprintf(debg_ofp, "TX%: %u\t",
	    (serv_stats.ss_tx*100)/serv_stats.t_tx);
    fprintf(debg_ofp, "RX%: %u\n",
	    (serv_stats.ss_rx*100)/serv_stats.t_rx);
    fprintf(debg_ofp, "Congestion Avoidance Packets Stats:\n");
    fprintf(debg_ofp, "TX: %u\t", serv_stats.ca_tx);
    fprintf(debg_ofp, "RX: %u\t", serv_stats.ca_rx);
    fprintf(debg_ofp, "TX%: %u\t",
	    (serv_stats.ca_tx*100)/serv_stats.t_tx);
    fprintf(debg_ofp, "RX%: %u\n",
	    (serv_stats.ca_rx*100)/serv_stats.t_rx);
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
    if (ctx->payload_length != 0) {
	strncpy(ctx->payload, pkt.payload, ctx->payload_length);
    } else {
	ctx->payload[0] = '\0';
    }
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
    fprintf(debg_ofp, "\n+++++++++++++Packet Seq: %d++++++++++++++++\n",
	    ctx.seq_no);
    fprintf(debg_ofp, "Message :\nAck : %s \n"\
	    "Data Pkt: %s\nLast Pkt: %s\n"\
	    "Advertised Window : %d \nAck No: %d \nSeq No: %d\n"\
	    "Payload Length: %d \nPayload: %s\n",
	    (ctx.is_ack == TRUE)?"TRUE":"FALSE",
	    (ctx.is_data_pkt == TRUE)?"TRUE":"FALSE",
	    (ctx.is_last_pkt == TRUE)?"TRUE":"FALSE",
	    ctx.awnd, ctx.ack_no, ctx.seq_no,
	    ctx.payload_length, ctx.payload);
    fprintf(debg_ofp, "\n+++++++++++++++++++++++++++++++++++++++++++\n");
    return 0;
}

/* int ruft_server_free_ctx(ruft_pkt_ctx_t *ctx) */
/* { */
/*     free(ctx->payload); */
/*     ctx->payload = NULL; */
/*     return 0; */
/* } */
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

int ruft_server_recv_pkt_with_timeout(ruft_server_rqst_info_t *req_info,
				      int wnd, int offset, FILE *debg_ofp)
{
    ruft_pkt_info_t pkt;
    ruft_pkt_ctx_t  ctx;
    int             num_bytes;
    int             status;
    struct timeval  recv_timeout;
    fd_set          sock_set;
    int             recv_ctr = offset;
    int             start_select = TRUE;

    bzero(&pkt, sizeof(pkt));
    fprintf(debg_ofp, "Time out : %lu\n", timeout);
    recv_timeout.tv_sec = 0;
    recv_timeout.tv_usec = timeout;
    while((recv_ctr < (offset + wnd)) && start_select == TRUE) {
	FD_SET(udp_serv_sock_fd, &sock_set);

	status = select(udp_serv_sock_fd+1, &sock_set,
			NULL, NULL, &recv_timeout);
	if (status == 0){
	    fprintf(debg_ofp, "ERROR time out reached\n");
	    fflush(debg_ofp);
	    return 1;
	}

	if (FD_ISSET(udp_serv_sock_fd, &sock_set)) {
	    start_select = FALSE;
	    while ((recv_ctr < (offset + wnd)) && start_select == FALSE)
	    {
		num_bytes = recvfrom(udp_serv_sock_fd, &pkt, sizeof(pkt), 0,
				     (struct sockaddr *)&(req_info->cli_addr),
				     (socklen_t *)&(req_info->cli_len));
		if (num_bytes == 0) {
		    fprintf(debg_ofp,
			    "UDP client connection closed at client side\n");
		    start_select = TRUE;
		    /*Come out of inner while loop and poll select*/
		    /*if the ctr cond is valid*/
		    continue;
		}
	
		if (num_bytes < 0) {
		    fprintf(debg_ofp, "ERROR: in UDP connection\n");
		    fflush(debg_ofp);
		    return -1;
		}
		ruft_server_pkt_info_to_ctx(pkt, &ctx, debg_ofp);
		ruft_server_print_pkt_ctx(ctx, debg_ofp);
		fprintf(debg_ofp, "Pkt Recvd: %d\n", ctx.ack_no);
		ruft_server_add_rx_stat();
		if (ctx.is_last_pkt == TRUE) {
		    traff_info[ctx.ack_no/MAX_PAYLOAD + 1].no_ack_recvd =
			traff_info[ctx.ack_no/MAX_PAYLOAD +1].no_ack_recvd + 1;
		    ruft_server_set_ack_recv(ctx);
		} else {
		    ruft_server_set_ack_recv(ctx);
		}
		recv_ctr++;
		fprintf(debg_ofp, "In inner while loop at %s\n", __FUNCTION__);
		start_select = TRUE;
	    }

	}
	fprintf(debg_ofp, "In outer while loop at %s\n", __FUNCTION__);
	FD_ZERO(&sock_set);
	FD_CLR(udp_serv_sock_fd, &sock_set);
    }
    return 0;
}

int ruft_server_process_req(ruft_pkt_ctx_t ctx,
			    ruft_server_rqst_info_t *rqst_info, FILE *debg_ofp)
{
    char method[10];
    FILE *uri_file_p;
    server_state = SV_PROC_REQ;
    ruft_server_print_server_state(stdout);
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
    if (index == 1) {
 	traff_info[index-1].seg_no = index - 1;
	traff_info[index-1].no_ack_recvd = 1;
	traff_info[index-1].is_acked = TRUE;
	traff_info[index-1].first_byte = ctx.seq_no;
	traff_info[index-1].last_byte = ctx.payload_length + ctx.seq_no -1;
    }
    traff_info[index].seg_no = index;
    traff_info[index].no_ack_recvd = 0;
    traff_info[index].is_acked = FALSE;
    traff_info[index].first_byte = ctx.seq_no;
    traff_info[index].last_byte = ctx.payload_length + ctx.seq_no -1;
    return 0;
}

int ruft_server_set_ett_timeout(unsigned long rtt)
{
    static first_time = TRUE;
    if (first_time == TRUE) {
	est_rtt = rtt;
	first_time = FALSE;
    }
    /*The Jacobson/Karels algo according to the book*/
    est_rtt = (0.875)*est_rtt + (0.125)*(rtt);
    dev_rtt = (0.75)*dev_rtt + (0.25)*(abs(rtt-est_rtt));
    timeout = est_rtt + 4*dev_rtt;
    return 0;    
}
int ruft_server_set_sent_time(unsigned int index)
{
    if (index == 1) {
	gettimeofday(&(traff_info[index-1].sent_time), 0);
    }
    gettimeofday(&(traff_info[index].sent_time), 0);
    return 0;
}
int ruft_server_set_ack_recv_time(unsigned int index)
{
    unsigned long int rtt;
    gettimeofday(&(traff_info[index].ack_recv_time), 0);
    traff_info[index].rtt = (traff_info[index].ack_recv_time.tv_sec
			     -traff_info[index].sent_time.tv_sec)*1000000
	+ traff_info[index].ack_recv_time.tv_usec
	- traff_info[index].sent_time.tv_usec;
    rtt = traff_info[index].rtt;
    ruft_server_set_ett_timeout(rtt);
    return 0;
}
int ruft_server_set_ack_recv(ruft_pkt_ctx_t ctx)
{
    int index = 1;
    int wnd = (ctx.ack_no / MAX_PAYLOAD);

    for (index = 1 ; index <= wnd; index++) {
	if (traff_info[index].no_ack_recvd == 0) {
	    traff_info[index].no_ack_recvd =
		traff_info[index].no_ack_recvd + 1;
	    no_dist_ack_recvd = no_dist_ack_recvd + 1;
	    traff_info[index].is_acked = TRUE;
	    ruft_server_set_ack_recv_time(index);
	    continue;
	}
	if (traff_info[index].last_byte == (ctx.ack_no - 1)) {
	    if (traff_info[index].no_ack_recvd == 0) {
		no_dist_ack_recvd = no_dist_ack_recvd + 1;
		traff_info[index].is_acked = TRUE;
	    }
	    traff_info[index].no_ack_recvd =
		traff_info[index].no_ack_recvd + 1;
	    continue;
	}
    }
    
    return 0;
    
}
int ruft_server_set_ack_recv_first_ele()
{
    traff_info[0].no_ack_recvd = traff_info[0].no_ack_recvd + 1;
    if (traff_info[0].no_ack_recvd < 2) {
	ruft_server_set_ack_recv_time(0);
    }
    return 0;
    
}
long int ruft_server_get_file_size(FILE *uri_file_p)
{
    long int size;
    fseek(uri_file_p, 0, SEEK_END);
    size = ftell(uri_file_p);
    fseek(uri_file_p, 0, SEEK_SET);
    return size;
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
    max_wd = no_of_segments+1;

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
    bzero(traff_info, max_wd*sizeof(ruft_server_traff_info_t));
    return 0;
}
int ruft_server_send_file_seg(ruft_pkt_ctx_t req_ctx,
			      ruft_server_rqst_info_t rqst_info,
			      int is_last_pkt,
			      unsigned int index, FILE *debg_ofp)
{
    ruft_pkt_ctx_t  reply_ctx;
    int             status = 0;
    FILE            *uri_file_p;
    int             last_byte = MAX_PAYLOAD;

    bzero(&reply_ctx, sizeof(reply_ctx));

    uri_file_p = fopen(rqst_info.file_name, "r");
    if (is_last_pkt != TRUE) {
	fseek(uri_file_p, (index-1)*MAX_PAYLOAD, SEEK_SET);
	last_byte = (index-1)*MAX_PAYLOAD + MAX_PAYLOAD;
	reply_ctx.payload_length = MAX_PAYLOAD;
    } else {
	fseek(uri_file_p, 0, SEEK_END);
	last_byte = ftell(uri_file_p);
	if (last_byte > (index-1)*MAX_PAYLOAD) {
	    fseek(uri_file_p, (index-1)*MAX_PAYLOAD, SEEK_SET);
	    reply_ctx.payload_length = last_byte - (index-1)*MAX_PAYLOAD;   
	}
    }
    reply_ctx.is_ack = TRUE;
    reply_ctx.is_data_pkt = TRUE;
    reply_ctx.is_last_pkt = is_last_pkt;
    
    reply_ctx.ack_no = req_ctx.seq_no + req_ctx.payload_length + index-1;
    reply_ctx.seq_no = ftell(uri_file_p);
    reply_ctx.awnd = MAX_PAYLOAD; //TODO
    
    fread(reply_ctx.payload, 1, reply_ctx.payload_length, uri_file_p);

    fseek(uri_file_p, 0, SEEK_SET);
    fclose(uri_file_p);

    ruft_server_add_traff_info(reply_ctx, index, debg_ofp);
    ruft_server_set_sent_time(index);
    status = ruft_server_send_pkt(reply_ctx, rqst_info, debg_ofp);
    fprintf(debg_ofp, "Sent pkt: %d\n", reply_ctx.seq_no);
    ruft_server_add_tx_stat();
    return status;
}

int ruft_server_send_file_size(ruft_pkt_ctx_t req_ctx,
			       ruft_server_rqst_info_t rqst_info,
			       unsigned int index,FILE *debg_ofp)
{
    ruft_pkt_ctx_t  reply_ctx;
    int             status = 0;
    char            buf[MAXLINE];
    FILE            *uri_file_p;
    
    uri_file_p = fopen(rqst_info.file_name, "r");
    sprintf(buf, "file_size: %ld", ruft_server_get_file_size(uri_file_p));
    fclose(uri_file_p);
    
    reply_ctx.is_ack = TRUE;
    reply_ctx.is_data_pkt = FALSE;
    reply_ctx.is_last_pkt = FALSE;
    
    reply_ctx.ack_no = req_ctx.seq_no+req_ctx.payload_length;
    reply_ctx.seq_no = req_ctx.ack_no;
    reply_ctx.awnd = MAX_PAYLOAD; //TODO
    reply_ctx.payload_length = strlen(buf)+1;
    strncpy(reply_ctx.payload, buf,
	    reply_ctx.payload_length-1);
    reply_ctx.payload[reply_ctx.payload_length-1] = '\0';
    ruft_server_add_traff_info(reply_ctx, index, debg_ofp);
    ruft_server_set_sent_time(index);
    status = ruft_server_send_pkt(reply_ctx, rqst_info, debg_ofp);
    return status;
    
}

int is_all_ack_recvd(int wnd, int offset)
{
    int index = offset;

    for (index = offset ; index < (offset+wnd); index++) {
	if (traff_info[index].no_ack_recvd == 0) {
	    fprintf(debg_ofp,"Chk ack Seg: %d\n", traff_info[index].seg_no);
	    return FALSE;
	}
    }
    
    return TRUE;    
}
int ruft_server_reconfigure_wnd()
{

    switch(server_state)
    {
    case SV_SLOW_START:
	ss_thresh = cwnd/2;
	if (ss_thresh == 0) {
	    ss_thresh = MAX_PAYLOAD;
	}
	cwnd = MAX_PAYLOAD;
	cwnd_seg = cwnd/MAX_PAYLOAD;
	break;
    case SV_CONG_AVOID:
	server_state = SV_SLOW_START;
	ss_thresh = cwnd/2;
	if (ss_thresh == 0) {
	    ss_thresh = MAX_PAYLOAD;
	}
	cwnd = MAX_PAYLOAD;
	cwnd_seg = cwnd/MAX_PAYLOAD;
	break;
    case SV_FAST_RECOV:
	ss_thresh = cwnd/2;
	if (ss_thresh == 0) {
	    ss_thresh = MAX_PAYLOAD;
	}
	cwnd = ss_thresh + 3*MAX_PAYLOAD;
	cwnd_seg = cwnd/MAX_PAYLOAD;
	server_state = SV_CONG_AVOID;
	break;
    }
    ruft_server_print_server_state(stdout);
    return 0;
}
int ruft_server_send_pending_data_segments(ruft_pkt_ctx_t req_ctx,
					   ruft_server_rqst_info_t rqst_info,
					   int wnd, int offset, FILE *debg_ofp)
{
    int send_ctr = offset;
    int is_last_pkt = FALSE;
    int first_time = TRUE;
    fprintf(debg_ofp, "In func %s \n", __FUNCTION__);

    while (send_ctr < (offset+wnd))
    {
	if (send_ctr == (max_wd -1)) {
	    is_last_pkt = TRUE;
	}
	if (traff_info[send_ctr].no_ack_recvd >= 3) {
	    traff_info[send_ctr].no_ack_recvd = 0;
	    traff_info[send_ctr].is_acked = FALSE;
	    ruft_server_send_file_seg(req_ctx, rqst_info, is_last_pkt,
				      send_ctr, debg_ofp);
	    fprintf(debg_ofp, "Retransmision for %d\n",
		    traff_info[send_ctr].first_byte);
	    server_state = SV_FAST_RECOV;
	    ruft_server_print_server_state(stdout);
	    if (first_time == TRUE) {
		ruft_server_reconfigure_wnd();
		first_time = FALSE;
	    }
	    return 0;
	}
	if(traff_info[send_ctr].no_ack_recvd == 0) {
	   ruft_server_send_file_seg(req_ctx, rqst_info, is_last_pkt,
				     send_ctr, debg_ofp);
	   if(first_time == TRUE) {
	       ruft_server_reconfigure_wnd();
	       first_time = FALSE;
	   }
	}
	send_ctr++;
	fprintf(debg_ofp, "In while loop at %s\n", __FUNCTION__);
    }
}
int ruft_server_chk_timeout_threshold(unsigned int count)
{
    if(count > THRESHOLD) {
	return TRUE;
    }
    return FALSE;
}
int ruft_server_send_file_seg_win(ruft_pkt_ctx_t req_ctx,
				  ruft_server_rqst_info_t rqst_info,
				  int wnd, int offset, FILE *debg_ofp)
{
    int ctr = offset;
    int is_last_pkt = FALSE;
    int all_ack_recvd = FALSE;
    int status = 0;
    unsigned int timeout_count = 0;
    
    if (ctr == (max_wd - 1)) {
	is_last_pkt = TRUE;
    }
    fprintf(debg_ofp, "wnd: %d offset: %d\n", wnd, offset);
    while (ctr < (offset + wnd) && is_last_pkt == FALSE)
    {
	ruft_server_send_file_seg(req_ctx, rqst_info,
				  is_last_pkt, ctr, debg_ofp);
	fprintf(debg_ofp, "In first while loop at %s seg %d\n",
		__FUNCTION__, ctr);
	ctr++;
	if(ctr == (max_wd - 1)) {
	    is_last_pkt = TRUE;
	}
    }
    if (is_last_pkt == TRUE) {
	ruft_server_send_file_seg(req_ctx, rqst_info,
				  is_last_pkt, ctr, debg_ofp);
	ctr++;
    }
    while(all_ack_recvd == FALSE) {
	status = ruft_server_recv_pkt_with_timeout(&rqst_info, wnd,
						   offset, debg_ofp);
	if (status == -1) {
	    return status;
	}
	if (status == 1) {
	    timeout_count++;
	    fprintf(debg_ofp, "Ack recv loop timed out count:%u\n",
		    timeout_count);
	    if(ruft_server_chk_timeout_threshold(timeout_count) == TRUE) {
		all_ack_recvd = TRUE;
		server_state = SV_FILE_SENT;
		fprintf(stdout, "Time out threshold reached\n");
		ruft_server_print_server_state(stdout);
		continue;
	    }
	}
	all_ack_recvd = is_all_ack_recvd(wnd, offset);
	if (all_ack_recvd == FALSE) {
	    ruft_server_send_pending_data_segments(req_ctx, rqst_info,
						   wnd, offset, debg_ofp);
	}
	fprintf(debg_ofp, "In second while loop at %s\n", __FUNCTION__);
    }
    return 0;
}
int ruft_server_print_server_state(FILE *debg_ofp)
{
    switch(server_state)
    {
    case SV_SLOW_START:
	fprintf(debg_ofp, "Server State: SLOW START\n");
	break;
    case SV_CONG_AVOID:
	fprintf(debg_ofp, "Server State: CONGESTION AVOIDANCE\n");
	break;
    case SV_REPLY_ERR:
	fprintf(debg_ofp, "Server State: ERROR REPLY\n");
	break;
    case SV_FILE_SENT:
	fprintf(debg_ofp, "Server State: FILE SENT\n");
	break;
    case SV_FAST_RECOV:
	fprintf(debg_ofp, "Server State: FAST RECOVERY\n");
	break;
    case SV_WAIT:
	fprintf(debg_ofp, "Server State: SERVER WAIT\n");
	break;
    case SV_PROC_REQ:
	fprintf(debg_ofp, "Server State: PROCESS REQUEST\n");
	break;
    default:
	fprintf(debg_ofp, "Server State: SERVER WAIT\n");
	break;
    }
    return 0;
}
int min(int a, int b)
{
    if (a >= b) {
	return b;
    }
    if (b > a) {
	return a;
    }
}
int ruft_server_get_wnd (FILE *debg_ofp)
{
    switch(server_state)
    {
    case SV_SLOW_START:
	cwnd = cwnd + MAX_PAYLOAD*no_dist_ack_recvd;
	cwnd_seg = cwnd/MAX_PAYLOAD;
	break;
    case SV_CONG_AVOID:
	cwnd = cwnd + (MAX_PAYLOAD*(MAX_PAYLOAD/cwnd))*no_dist_ack_recvd;
	cwnd_seg = cwnd/MAX_PAYLOAD;
	break;
    }
    if (cwnd > ss_thresh && server_state == SV_SLOW_START) {
	server_state = SV_CONG_AVOID;
    }
    ruft_server_print_server_state(stdout);
    fprintf(debg_ofp, "cwnd_seg: %d Dist_ack_recvd: %d \n",
	    cwnd_seg, no_dist_ack_recvd);
    return min(cwnd_seg, rwnd_seg);
}
int ruft_server_wnd_init()
{
    cwnd = MAX_PAYLOAD;
    cwnd_seg = 1;
    ss_thresh = 50*MAX_PAYLOAD;
    ss_thresh_seg = 50;
    no_dist_ack_recvd = 0;
}
int ruft_server_send_file(ruft_pkt_ctx_t req_ctx,
			  ruft_server_rqst_info_t rqst_info, FILE *debg_ofp)
{
    int wnd_ctr = 1;
    int wnd = 1;
    int temp_wnd = 5;
    server_state = SV_SLOW_START;
    ruft_server_print_server_state(stdout);
    ruft_server_wnd_init();
    wnd = ruft_server_get_wnd(debg_ofp);
    ruft_server_create_traff_window(rqst_info, debg_ofp);
    ruft_server_send_file_size(req_ctx, rqst_info, 0, debg_ofp);
    ruft_server_add_tx_stat();
    while (server_state != SV_FILE_SENT && wnd_ctr < max_wd)
    {
	if ((max_wd - wnd_ctr) <= (wnd - wnd_ctr)) {
	    wnd = max_wd - wnd_ctr;
	}
	fprintf(debg_ofp, "wnd: %d offset: %d\n", wnd, wnd_ctr);
	ruft_server_send_file_seg_win(req_ctx, rqst_info,
				      wnd, wnd_ctr, debg_ofp);
	wnd_ctr = wnd_ctr + wnd;
	wnd = ruft_server_get_wnd(debg_ofp);
	if (wnd > max_wd) {
	    wnd = max_wd;
	}
	if ((wnd + wnd_ctr)>max_wd) {
	    wnd = max_wd-wnd_ctr;
	}
	if (is_all_ack_recvd(max_wd, 0) == TRUE) {
	    server_state = SV_FILE_SENT;
	    ruft_server_print_server_state(stdout);
	    fprintf(stdout, "File Sent\n");
	}
	fprintf(debg_ofp, "In while loop at %s\n", __FUNCTION__);
    }
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
    ruft_server_print_server_state(stdout);
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
	ruft_server_set_ack_recv_first_ele();
	fprintf(debg_ofp, "\nRTT: %lu Timeout: %lu\n",
		traff_info[0].rtt, timeout);
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
    fprintf(debg_ofp, "Request Recieved\n");
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
    server_state = SV_WAIT;
    ruft_server_print_server_state(stdout);
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

    if (argc < 3) {
 	fprintf(debg_ofp, "No port no or rwnd entered hence exiting\n"\
		"Syntax: ./ruft_server.bin <port-no>\t"\
		"<rwnd in multiples of 1280>");
	fprintf(stderr, "Very Few Arguments enter port no "\
		"and rwnd\n");
	fflush(debg_ofp);
	cleanup(-1, debg_ofp);
	exit(1);
    }
    port_no = atoi(argv[1]);
    rwnd    = atoi(argv[2]);
    rwnd_seg = rwnd/MAX_PAYLOAD;
    status = ruft_server_bootstrap(&udp_serv_sock_fd, port_no, debg_ofp);
    if (status != 0){
	fprintf(debg_ofp, "ERROR in binding server port\n");
	fprintf(stderr,
		"ERROR bootstrapping server see log files for details\n");
	fflush(debg_ofp);
	cleanup(udp_serv_sock_fd, debg_ofp);
	exit(status);
    }
    server_state = SV_WAIT;
    ruft_server_print_server_state(stdout);
    while(server_state == SV_WAIT)
    {
	bzero(&serv_stats, sizeof(ruft_server_stats_t));
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
	ruft_server_add_rx_stat();
	ruft_server_handle_pkt(pkt, cli_addr, cli_len, debg_ofp);
	ruft_server_print_stat(stdout);
	fprintf(debg_ofp, "In main while loop\n");
	
    }
    cleanup(udp_serv_sock_fd, debg_ofp);
}
