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


#define MAXLINE 20
#define WAIT    10
#define MAX_BUFFER 1200
#define MAX_PAYLOAD 1000

FILE *debg_ofp;
int udp_serv_sock_fd;
static volatile int keep_running = 1;
typedef struct http_packet_info_
{
    char method[MAXLINE];
    char uri[MAXLINE];
    char version[MAXLINE];
} http_packet_info_t;

/*
 * Free the return value after use
 */
char *get_file_from_uri(char *uri)
{
    char *file_name;
    file_name = malloc(sizeof(char)*MAXLINE);
    strncpy(file_name, uri+1,MAXLINE);
    return file_name;
}

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

int parse_http_request(char *req, http_packet_info_t *req_info,
		       FILE *debg_ofp)
{

    sscanf(req, "%s\t%s\t%s", req_info->method,
	   req_info->uri, req_info->version);

    fprintf(debg_ofp,
	    "INFO: Parsed Info- Method: %s\tURI: %s\t" \
            "Version: %s\n",
	    req_info->method, req_info->uri,
	    req_info->version);
    return 0;
}

int build_http_get_err_response(char *resp_buf, FILE *debg_ofp)
{
    char resp_msg[MAX_BUFFER];
    sprintf(resp_msg,"<html><title>Arya Error</title>" \
	    "<body><h1>404 Page not Found</h1></body></html>");
  
    sprintf(resp_buf, "HTTP/1.0 %s %s\r\n", "404", "Not Found");
    sprintf(resp_buf, "%sContent-type: text/html\r\n", resp_buf);
    sprintf(resp_buf, "%sContent-length: %lu\r\n\r\n", resp_buf,
	    strlen(resp_msg));
    sprintf(resp_buf, "%s%s", resp_buf, resp_msg);
    fprintf(debg_ofp, "INFO: GET Response: \n %s\n", resp_buf);
    return 0;
  
}

int build_http_get_response(http_packet_info_t req_info,
			    char *resp_buf, FILE *debg_ofp)
{
    char        *file_name;
    int         status=0;
    FILE        *uri_file_p;
    int         uri_file_fd;
    struct stat f_stat;
    char        *file_in_str;
    file_name = get_file_from_uri(req_info.uri);
    uri_file_p = fopen(file_name, "r");
    if (uri_file_p == NULL) {
	fprintf(debg_ofp, "ERROR opening file %s not found\n", file_name);
	return 1;
    }
    uri_file_fd = fileno(uri_file_p);
    fstat(uri_file_fd, &f_stat);
    if (f_stat.st_size>MAX_PAYLOAD) {
	fclose(uri_file_p);
	return 2;
    }
    sprintf(resp_buf, "HTTP/1.0 200 OK\r\n");
    sprintf(resp_buf, "%sServer: Stark Web Server\r\n", resp_buf);
    sprintf(resp_buf, "%sContent-length: %lld\r\n", resp_buf, f_stat.st_size);
    sprintf(resp_buf, "%sContent-type: %s\r\n\r\n", resp_buf, "text/html");

    file_in_str = malloc(f_stat.st_size);
    fseek(uri_file_p, 0, SEEK_SET);
    fread(file_in_str, 1, f_stat.st_size, uri_file_p);
    fclose(uri_file_p);
    sprintf(resp_buf, "%s%s\r\n", resp_buf, file_in_str);
    fprintf(debg_ofp, "INFO: GET Response: \n %s\n", resp_buf);
    return 0;
}
int segment_and_send_response(int conn_sock_fd, http_packet_info_t req_info,
			      struct sockaddr_in cli_addr, int cli_len,
			      FILE *debg_ofp)
{
    int         i;
    char        *file_name;
    int         status=0;
    FILE        *uri_file_p;
    int         uri_file_fd;
    struct stat f_stat;
    char        *file_in_str;
    int         no_of_segments;
    int         remainder = 0;
    char        resp_buf[MAX_BUFFER];
    
    file_name = get_file_from_uri(req_info.uri);
    uri_file_p = fopen(file_name, "r");
    if (uri_file_p == NULL) {
	fprintf(debg_ofp, "ERROR opening file %s not found\n", file_name);
	return 1;
    }
    uri_file_fd = fileno(uri_file_p);
    fstat(uri_file_fd, &f_stat);
    no_of_segments = f_stat.st_size/MAX_PAYLOAD;
    if (f_stat.st_size % MAX_PAYLOAD != 0) {
	no_of_segments = no_of_segments + 1;
	remainder = f_stat.st_size % MAX_PAYLOAD;
    }
    fseek(uri_file_p, 0, SEEK_SET);
    file_in_str = malloc(MAX_PAYLOAD);
    for (i = 0; i<no_of_segments; i++)
    {
	sprintf(resp_buf, "HTTP/1.0 200 OK\r\n");
	sprintf(resp_buf, "%sServer: Stark Web Server\r\n", resp_buf);
	sprintf(resp_buf, "%sContent-length: %lld\r\n", resp_buf, f_stat.st_size);
	sprintf(resp_buf, "%sContent-type: %s\r\n", resp_buf, "text/html");
	sprintf(resp_buf, "%sSegment: %d %d\r\n\r\n",
		resp_buf, i+1, no_of_segments);

        if (i == (no_of_segments-1)) {
	    if (remainder != 0) {
		fread(file_in_str, 1, remainder, uri_file_p);
	    } else {
		fread(file_in_str, 1, MAX_PAYLOAD, uri_file_p);
	    }
	} else {
	    fread(file_in_str, 1, MAX_PAYLOAD, uri_file_p);
	}
	sprintf(resp_buf, "%s%s\r\n", resp_buf, file_in_str);
	status = sendto(conn_sock_fd, resp_buf, strlen(resp_buf),
			0, (struct sockaddr *)&cli_addr, cli_len);
	fprintf(debg_ofp, "INFO: GET Response: \n %s\n", resp_buf);
	bzero(resp_buf, MAX_BUFFER);
	bzero(file_in_str, MAX_PAYLOAD);
    }
    free(file_in_str);
    fclose(uri_file_p);
    return 0;
    
}

int respond_to_http(int conn_sock_fd, http_packet_info_t req_info,
		    struct sockaddr_in cli_addr, int cli_len, FILE *debg_ofp)
{
    char               resp_buf[MAX_BUFFER];
    int                status;
    
    status = build_http_get_response(req_info, resp_buf, debg_ofp);
    switch(status)
    {
    case 0:
	status = sendto(conn_sock_fd, resp_buf, strlen(resp_buf),
			0, (struct sockaddr *)&cli_addr, cli_len);
	break;
    case 1:
	build_http_get_err_response(resp_buf, debg_ofp);
	status = sendto(conn_sock_fd, resp_buf, strlen(resp_buf),
			0, (struct sockaddr *)&cli_addr, cli_len);
	break;
    case 2:
	/*In case file requested is more than datagram buffer*/
	segment_and_send_response(conn_sock_fd, req_info,
				  cli_addr, cli_len, debg_ofp);
    default:
	return status;
    }
    fprintf(debg_ofp, "Sending status : %d\n", status);
    return status;
}

int handle_connection(int new_sock_conn, char *buf,
		      struct sockaddr_in cli_addr,
		      int cli_len, FILE *debg_ofp)
{
    int                   status = 0;
    http_packet_info_t    req_info;

 
    parse_http_request(buf, &req_info, debg_ofp);

    status = respond_to_http(new_sock_conn, req_info,
			     cli_addr, cli_len, debg_ofp);
    fprintf(debg_ofp, "DEBUG: Error in replying back to UDP\n");
    fflush(debg_ofp);
    return status;
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
	num_bytes = recvfrom(udp_serv_sock_fd, buf, MAX_BUFFER, 0,
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
	fprintf(debg_ofp, "Recieved Message :\n %s", buf);
	
	status = handle_connection(udp_serv_sock_fd, buf,
				   cli_addr, cli_len, debg_ofp);
	if (status != 0){
	    fprintf(debg_ofp, "ERROR in replying back %d\n", status);
	    fprintf(stderr,
		    "ERROR in UDP reply see log files for details %d\n", status);
	    fflush(debg_ofp);
	    continue;
	}
    }
    cleanup(udp_serv_sock_fd, debg_ofp);
}
