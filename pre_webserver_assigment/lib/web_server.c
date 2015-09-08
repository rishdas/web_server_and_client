#include<sys/socket.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>


#define MAXLINE 2000
typedef struct http_packet_info_
{
    char method[MAXLINE];
    char uri[MAXLINE];
    char version[MAXLINE];
} http_packet_info_t;

int parse_http_request(char *req, http_packet_info_t *req_info,
		       FILE *debg_ofp)
{
    sscanf(req, "%s %s %s", req_info->method,
	   req_info->uri, req_info->version);
    fprintf(debg_ofp,
	    "INFO: Parsed Info- Method: %s\tURI: %s\tVersion: %s\t\n",
	    req_info->method, req_info->uri, req_info->version);
    return 0;
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
			
int bootstrap_server(int *serv_sock_fd, int port_no, FILE *debg_ofp)
{
    int                status;
    struct sockaddr_in serv_addr;
  
    *serv_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
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
	fprintf(debg_ofp, "ERROR opening file file not found\n");
	return 1;
    }
    uri_file_fd = fileno(uri_file_p);
    fstat(uri_file_fd, &f_stat);
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
int build_http_get_err_response(char *resp_buf, FILE *debg_ofp)
{
    char resp_msg[8192];
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
int respond_to_http(int conn_sock_fd,
		    http_packet_info_t req_info, FILE *debg_ofp)
{
    char resp_buf[8192];
    int  status;
    status = build_http_get_response(req_info, resp_buf, debg_ofp);
    switch(status)
    {
    case 0:
	send(conn_sock_fd, resp_buf, strlen(resp_buf), 0);
	break;
    case 1:
	build_http_get_err_response(resp_buf, debg_ofp);
	send(conn_sock_fd, resp_buf, strlen(resp_buf), 0);
	break;
    default:
	return status;
    }
    return status;
}
int handle_connection(int new_sock_conn, struct sockaddr_in cli_addr,
		      int cli_len, FILE *debg_ofp)
{
    int                   status = 0;
    char                  buf[8192];
    http_packet_info_t    req_info;

  
    bzero(buf, 8192);
    status = read(new_sock_conn, buf, 8192);
    if (status < 0) {
	fprintf(debg_ofp, "ERROR: in reading from socket\n");
	return 1;
    }
    fprintf(debg_ofp, "INFO: Incoming message:\n%s\n",buf);
    parse_http_request(buf, &req_info, debg_ofp);
    status = respond_to_http(new_sock_conn, req_info, debg_ofp);
    close(new_sock_conn);
    return status;
}

  


  
