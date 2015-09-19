#include<sys/socket.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/time.h>


#define MAXLINE 2000
#define WAIT    10
typedef struct http_packet_info_
{
    char method[MAXLINE];
    char uri[MAXLINE];
    char version[MAXLINE];
    int  is_keepalive;
} http_packet_info_t;

int parse_http_request(char *req, http_packet_info_t *req_info,
		       FILE *debg_ofp)
{
    char connection_status[MAXLINE];
    char parsed_field[MAXLINE];
    char parsed_value[MAXLINE];
    int  bytes_read;
    int  connection_found = 0;
    
    sscanf(req, "%s %s %s\r\n%n", req_info->method,
	   req_info->uri, req_info->version, &bytes_read);
    while(connection_found == 0) {
	req = req + bytes_read;
	sscanf(req, "%s %s%n", parsed_field, parsed_value, &bytes_read);
	if (strncmp("Connection:", parsed_field, MAXLINE) == 0) {
	    connection_found = 1;
	}
    }
    if (connection_found == 1) {
	if (strncmp(parsed_value, "keep-alive",
		    strlen(parsed_value)) == 0) {
	    req_info->is_keepalive = 0;
	} else {
	    req_info->is_keepalive = 1;
	}
    }

    fprintf(debg_ofp,
	    "INFO: Parsed Info- Method: %s\tURI: %s\t" \
            "Version: %s\tConnection: %s\n",
	    req_info->method, req_info->uri,
	    req_info->version,
	    (req_info->is_keepalive == 0)?"keep-alive":"close");
    return 0;
}

int is_persistent(http_packet_info_t req_info)
{
    if (strncmp(req_info.version, "HTTP/1.0",
		strlen(req_info.version)) == 0) {
	return 1;
    }
    else
    {
	return 0;
    }
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

int build_http_get_response_persitant(http_packet_info_t req_info,
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
    sprintf(resp_buf, "HTTP/1.1 200 OK\r\n");
    sprintf(resp_buf, "%sServer: Stark Web Server\r\n", resp_buf);
    sprintf(resp_buf, "%sContent-length: %lld\r\n", resp_buf, f_stat.st_size);
    sprintf(resp_buf, "%sKeep-Alive: timeout=10, max=5\r\n", resp_buf);
    sprintf(resp_buf, "%sConnection: Keep-Alive\r\n", resp_buf);
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
    if (is_persistent(req_info) != 0) {
	status = build_http_get_response(req_info, resp_buf, debg_ofp);
    } else {
	status = build_http_get_response_persitant(req_info,
						   resp_buf, debg_ofp);
    }
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

int wait_for_and_hdl_persistant_conn(int new_sock_conn,
				     http_packet_info_t req_info,
                                     FILE *debg_ofp)
{
    struct timeval    timeout;
    fd_set            sock_set;
    int               status;
    char              buf[8192];
    ssize_t           recvd_bytes;

    FD_ZERO(&sock_set);
    timeout.tv_sec = WAIT;
    timeout.tv_usec = 0;
    // FD_SET(new_sock_conn, &sock_set);
    
    /*Respond to the first persistant connection*/
    status = respond_to_http(new_sock_conn, req_info, debg_ofp);
    if (status != 0) {
	return status;
    }
    while(1)
    {
	FD_SET(new_sock_conn, &sock_set);
	fprintf(debg_ofp, "DEBUG: in while loop fd :%d\n", new_sock_conn);
	status = select(new_sock_conn+1, &sock_set, NULL, NULL, &timeout);
	fprintf(debg_ofp, "DEBUG: post select status %d\n", status);
	if (status == 0) {
	    fprintf(debg_ofp, "ERROR: Persistant time out reached\n");
	    fflush(debg_ofp);
	    return status;
	} 
	if (FD_ISSET(new_sock_conn, &sock_set)) {
	    recvd_bytes = recv(new_sock_conn, buf, sizeof(buf), 0);
	    if (recvd_bytes <= 0) {
		fprintf(debg_ofp, "ERROR: Reading from socket\n");
	    }
	    buf[recvd_bytes]='\0';
	    fprintf(debg_ofp, "INFO: Received message\n %s", buf);
	    parse_http_request(buf, &req_info, debg_ofp);
	    status = respond_to_http(new_sock_conn, req_info, debg_ofp);

	    /*Reset Time*/
	    timeout.tv_sec = WAIT;
	    timeout.tv_usec = 0;
	    bzero(buf, 8192);
	    FD_CLR(new_sock_conn, &sock_set);
	}
	if (req_info.is_keepalive != 0) {
	    fflush(debg_ofp);
	    return status;
	}
	
    }
    fflush(debg_ofp);
    return status;
}
    
int handle_connection(int new_sock_conn, struct sockaddr cli_addr,
		      int cli_len, FILE *debg_ofp)
{
    int                   status = 0;
    char                  buf[8192];
    http_packet_info_t    req_info;

  
    bzero(buf, 8192);
    status = recv(new_sock_conn, buf, 8192, 0);
    if (status < 0) {
	fprintf(debg_ofp, "ERROR: in reading from socket\n");
	fflush(debg_ofp);
	return 1;
    }
    fprintf(debg_ofp, "INFO: Incoming message:\n%s\n",buf);
    parse_http_request(buf, &req_info, debg_ofp);
    if (is_persistent(req_info) != 0) {
	status = respond_to_http(new_sock_conn, req_info, debg_ofp);
	fprintf(debg_ofp, "DEBUG: Not Persistant connection\n");
	fflush(debg_ofp);
	close(new_sock_conn);
    } else {
	status = wait_for_and_hdl_persistant_conn(new_sock_conn,
						  req_info, debg_ofp);
	fprintf(debg_ofp, "DEBUG: Persistant connection status : %d\n",
		status);
	fflush(debg_ofp);
	close(new_sock_conn);
    }
    return status;
}

  


  
