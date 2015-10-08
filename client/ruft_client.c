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
ruft_client_traff_info_t *traff_info;
unsigned int max_wd;
unsigned long int est_rtt = 0;
unsigned long int dev_rtt = 0;
unsigned long int timeout = 0;

typedef struct client_info_
{
    int                port;
    char               *file_name;
    struct hostent     *server_addr;
    struct sockaddr_in serv_sock_addr;
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
int ruft_client_create_traff_window(client_info_t rqst_info,
				    FILE *debg_ofp)
{
    switch(client_state)
    {
    case CL_SEND_REQUEST:
	max_wd =1;
	break;
    case CL_RECV_ERROR:
	max_wd = 1;
	break;
    case CL_SLOW_START:
	max_wd = max_wd+1;
    default:
	break;
    }
    traff_info = (ruft_client_traff_info_t *)
	realloc(traff_info, max_wd*sizeof(ruft_client_traff_info_t));
    bzero(traff_info, max_wd*sizeof(traff_info));
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
int ruft_client_set_ett_timeout(unsigned long rtt)
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
int ruft_client_set_ack_sent_time(unsigned int index)
{
    gettimeofday(&(traff_info[index].ack_sent_time), 0);
    return 0;
}

int ruft_client_set_data_recv_time(unsigned int index)
{
    unsigned long int rtt;
    traff_info[index].no_data_recvd = traff_info[index].no_data_recvd + 1;
    if (traff_info[index].no_data_recvd < 2) {
	gettimeofday(&(traff_info[index].data_recv_time), 0);
	traff_info[index].rtt = (traff_info[index].data_recv_time.tv_sec
				 -traff_info[index].ack_sent_time.tv_sec)*1000000
	    + traff_info[index].data_recv_time.tv_usec
	    - traff_info[index].ack_sent_time.tv_usec;
	rtt = traff_info[index].rtt;
	ruft_client_set_ett_timeout(rtt);
    }
    return 0;
}

int ruft_client_set_ack_sent(unsigned int index)
{
    traff_info[index].no_ack_sent = traff_info[index].no_ack_sent + 1;
    if (traff_info[index].no_ack_sent < 2) {
	ruft_client_set_ack_sent_time(index);
    }
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
    if (ctx ->payload_length != 0) {
	strncpy(ctx->payload, pkt.payload, ctx->payload_length);
    } else {
	ctx->payload[0] = '\0';
    }
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
    if (ctx.payload_length != 0) {
	strncpy(pkt->payload, ctx.payload, ctx.payload_length);
    } else {
	pkt->payload[0] = '\0';
    }
    return 0;
}

int ruft_client_print_pkt_ctx(ruft_pkt_ctx_t ctx, FILE *debg_ofp)
{
    int i = 0;
    
    fprintf(stdout, "\n+++++++++++++Packet Seq: %d++++++++++++++++\n",
	    ctx.seq_no);
    fprintf(stdout, "Message :\nAck : %s \n"\
	    "Data Pkt: %s\nLast Pkt: %s\n"\
	    "Advertised Window : %d \nAck No: %d \nSeq No: %d\n"\
	    "Payload Length: %d \n",
	    (ctx.is_ack == TRUE)?"TRUE":"FALSE",
	    (ctx.is_data_pkt == TRUE)?"TRUE":"FALSE",
	    (ctx.is_last_pkt == TRUE)?"TRUE":"FALSE",
	    ctx.awnd, ctx.ack_no, ctx.seq_no,
	    ctx.payload_length);
    fprintf(stdout, "Payload: ");
    while(i<ctx.payload_length)
    {
	fprintf(stdout,"%c", ctx.payload[i]);
	i++;
    }
    fprintf(stdout, "\n");
    fprintf(stdout, "\n+++++++++++++++++++++++++++++++++++++++++++\n");
    return 0;
}

/* int ruft_client_free_ctx(ruft_pkt_ctx_t ctx) */
/* { */
/*     free(ctx.payload); */
/*     return 0; */
/* } */
int ruft_client_add_traff_info(ruft_pkt_ctx_t ctx, unsigned int index,
			       FILE *debg_ofp)
{
    traff_info[index].seg_no = index;
    traff_info[index].no_ack_sent = 0;
    traff_info[index].is_acked = FALSE;
    traff_info[index].first_byte = ctx.seq_no;
    traff_info[index].last_byte = ctx.payload_length + ctx.seq_no -1;
    return 0;
}
int ruft_client_build_get_rqst(ruft_pkt_ctx_t *ctx, client_info_t client_info,
			       FILE *debg_ofp)
{
    ctx->is_ack = FALSE;
    ctx->is_data_pkt = FALSE;
    ctx->is_last_pkt = FALSE;
    ctx->awnd = MAX_PAYLOAD; //TODO
    ctx->ack_no = 1;
    ctx->seq_no = 1;
    ctx->payload_length = strlen(client_info.file_name)+strlen("GET")+2;
    sprintf(ctx->payload, "GET %s", client_info.file_name);
    ctx->payload[ctx->payload_length-1] = '\0';
    ruft_client_print_pkt_ctx(*ctx, debg_ofp);
    return 0;
}
int ruft_client_send_pkt(ruft_pkt_ctx_t ctx, client_info_t client_info,
			 FILE *debg_ofp)
{
    ruft_pkt_info_t   pkt;
    int               status;
    
    bzero(&pkt, sizeof(pkt));

    ruft_client_pkt_ctx_to_info(&pkt, ctx, debg_ofp);
    status = sendto(client_sock_fd, &(pkt), sizeof(pkt), 0,
		    (struct sockaddr *)&(client_info.serv_sock_addr),
		    sizeof(client_info.serv_sock_addr));
    if (status < 0) {
	fprintf(stderr,
		"Error writing into the socket seq no:%d\n", ctx.seq_no);
	fprintf(debg_ofp,
		"Error writing into the socket seq no: %d\n", ctx.seq_no);
	close(client_sock_fd);
	return -1;
    }
    return 0;
}
int ruft_client_recv_pkt(ruft_pkt_ctx_t *ctx, client_info_t client_info,
			  FILE *debg_ofp)
{
    ruft_pkt_info_t    pkt;
    struct sockaddr_in server_addr;
    size_t             server_addr_len;
    int                status;
    
    bzero(&pkt, sizeof(pkt));

    status = recvfrom(client_sock_fd, &(pkt), sizeof(pkt), 0,
		      (struct sockaddr *)&server_addr,
		      (socklen_t *)&server_addr_len);
    if (status < 0) {
	fprintf(stderr, "Error recieving into the socket\n");
	fprintf(debg_ofp, "Error recieving into the socket\n");
	close(client_sock_fd);
	return -1;
    }
    ruft_client_pkt_info_to_ctx(pkt, ctx, debg_ofp);
    ruft_client_print_pkt_ctx(*ctx, debg_ofp);
    return 0;
}
int ruft_client_recv_pkt_with_timeout(ruft_pkt_ctx_t *ctx,
				      client_info_t client_info, FILE *debg_ofp)
{
    ruft_pkt_info_t    pkt;
    struct sockaddr_in server_addr;
    size_t             server_addr_len;
    int                status;
    struct timeval     recv_timeout;
    fd_set             sock_set;
    
    bzero(&pkt, sizeof(pkt));
    recv_timeout.tv_sec = 0;
    recv_timeout.tv_usec = timeout;
    FD_SET(client_sock_fd, &sock_set);
    status = select(client_sock_fd + 1, &sock_set, NULL, NULL, &recv_timeout);
    if (status == 0) {
	fprintf(stderr, "ERROR: time out reached\n");
	fflush(stderr);
	return status+1;
    } 
    if (FD_ISSET(client_sock_fd, &sock_set)) {

	status = recvfrom(client_sock_fd, &(pkt), sizeof(pkt), 0,
			  (struct sockaddr *)&server_addr,
			  (socklen_t *)&server_addr_len);
	if (status < 0) {
	    fprintf(stderr, "Error reading(timeout) from the socket\n");
	    fprintf(debg_ofp, "Error reading(timeout) from the socket\n");
	    close(client_sock_fd);
	    return -1;
	}
	ruft_client_pkt_info_to_ctx(pkt, ctx, debg_ofp);
	ruft_client_print_pkt_ctx(*ctx, debg_ofp);
	FD_CLR(client_sock_fd, &sock_set);
    }
    return 0;
}
int ruft_client_send_ack(ruft_pkt_ctx_t req_ctx, client_info_t client_info,
			 int is_last_pkt, FILE *debg_ofp)
{
    ruft_pkt_ctx_t ack_ctx;
    int            status;

    ack_ctx.is_ack = TRUE;
    ack_ctx.is_data_pkt = FALSE;
    ack_ctx.is_last_pkt = is_last_pkt;
    ack_ctx.ack_no = req_ctx.seq_no+req_ctx.payload_length;
    ack_ctx.seq_no = req_ctx.ack_no;
    ack_ctx.awnd = MAX_PAYLOAD;
    ack_ctx.payload_length = 1;
    ack_ctx.payload[0]='\0';
    status = ruft_client_send_pkt(ack_ctx, client_info, debg_ofp);
    return status;
}
int ruft_client_write_to_file(ruft_pkt_ctx_t ctx, client_info_t client_info,
			      FILE *debg_ofp)
{
    FILE *dest_file_p;
    unsigned int i = 0;
    static first_time = TRUE;
    if(first_time == TRUE) {
	dest_file_p = fopen(client_info.file_name, "w");
	first_time = FALSE;
    } else {
	dest_file_p = fopen(client_info.file_name, "a");
    }

    fwrite(ctx.payload, 1, ctx.payload_length, dest_file_p);
    
    fclose(dest_file_p);
    
    return 0;
}
int ruft_client_handle_reply(ruft_pkt_ctx_t req_ctx, client_info_t client_info,
			     FILE *debg_ofp)
{
    ruft_pkt_ctx_t ctx;
    ruft_pkt_ctx_t r_ctx;
    unsigned int   i;
    int            status = 0;

    bzero(&ctx, sizeof(ctx));
    
    status = ruft_client_recv_pkt_with_timeout(&ctx, client_info, debg_ofp);
    while (status == 1) {
	timeout = timeout + 500;
	ruft_client_set_ack_sent(0);
	ruft_client_send_pkt(req_ctx, client_info, debg_ofp);
	status = ruft_client_recv_pkt_with_timeout(&ctx, client_info, debg_ofp);
    }
    if (ctx.is_last_pkt == TRUE && ctx.is_ack == TRUE
	&& ctx.is_data_pkt == FALSE) {
	ruft_client_set_data_recv_time(0);
	fprintf(stdout, "RTT: %lu Timeout: %lu\n",
		traff_info[0].rtt, timeout);
	ruft_client_send_ack(ctx, client_info, TRUE, debg_ofp);
	client_state = CL_FILE_RCVD;
	return 0;
    }
    i = 1;
    client_state = CL_SLOW_START;
    while(client_state != CL_FILE_RCVD)
    {
//	if (traff_info[i-1].no_data_recvd < 2) {
	ruft_client_write_to_file(ctx, client_info, debg_ofp);
	ruft_client_set_data_recv_time(i-1);
	fprintf(stdout, "RTT: %lu Timeout: %lu\n",
		traff_info[i-1].rtt, timeout);
//	}	
	ruft_client_create_traff_window(client_info, debg_ofp);
	ruft_client_add_traff_info(ctx, i, debg_ofp);
	if (ctx.is_last_pkt != TRUE) {
	    ruft_client_send_ack(ctx, client_info, FALSE, debg_ofp);
	} else {
	    ruft_client_send_ack(ctx, client_info, TRUE, debg_ofp);
	    client_state = CL_FILE_RCVD;
	    ruft_client_set_ack_sent(i);
	    return 0;
	}
	ruft_client_set_ack_sent(i);
	r_ctx = ctx;
	bzero(&ctx, sizeof(ctx));
	status =
	    ruft_client_recv_pkt_with_timeout(&ctx, client_info, debg_ofp);
	if(status < 0) {
	    return status;
	}
	while(status == 1) {
	    fprintf (stdout, "Packet Retransmitted\n");
	    timeout = timeout + 500;
	    if (r_ctx.is_last_pkt != TRUE) {
		ruft_client_send_ack(r_ctx, client_info, FALSE, debg_ofp);
	    } else {
		ruft_client_send_ack(r_ctx, client_info, TRUE, debg_ofp);
		client_state = CL_FILE_RCVD;
		ruft_client_set_ack_sent(i);
		return 0;
	    }
	    ruft_client_set_ack_sent(i);
	    status =
		ruft_client_recv_pkt_with_timeout(&ctx, client_info, debg_ofp);
	    if (status < 0) {
		return status;
	    }
	}
	if (ctx.is_last_pkt == TRUE && ctx.is_ack == TRUE) {
	    ruft_client_write_to_file(ctx, client_info, debg_ofp);
	    ruft_client_set_data_recv_time(i);
	    fprintf(stdout, "RTT: %lu Timeout: %lu\n",
		    traff_info[0].rtt, timeout);
	    ruft_client_send_ack(ctx, client_info, TRUE, debg_ofp);
	    client_state = CL_FILE_RCVD;
	    return 0;
	}
	i++;

    }
    return 0;
}
int main(int argc, char *argv[])
{

    int                status;
    client_info_t      client_info;
    char               file_name[] = "udp_client_trace.log\0";
    struct sockaddr_in server_addr;
    ruft_pkt_ctx_t     ctx;

    traff_info = NULL;
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
    client_info.serv_sock_addr = server_addr;
    client_state = CL_SEND_REQUEST;
    while (client_state == CL_SEND_REQUEST) {
	ruft_client_build_get_rqst(&ctx, client_info, debg_ofp);
	ruft_client_create_traff_window(client_info, debg_ofp);
	ruft_client_add_traff_info(ctx, 0, debg_ofp);
	ruft_client_set_ack_sent(0);
	ruft_client_send_pkt(ctx, client_info, debg_ofp);
	ruft_client_handle_reply(ctx, client_info, debg_ofp);	
    }
    fclose(debg_ofp);
    return 0;
}



