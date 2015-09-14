#include<stdio.h>
#include<stdlib.h>
#include<netdb.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<unistd.h>
#include<strings.h>

FILE *debg_ofp;
int  client_sock_fd;
typedef struct client_info_
{
    int            port;
    char           *connection_type;
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
    if(argc< 5) {
	fprintf(debg_ofp, "ERROR: Very few arguments "\
		"the format for is \n"\
		"client\t<server_host>\t<server_port>" \
		"\t<connection_type>\t<filename.txt>\n");
	return -1;
	
    }
    
    client_info->server_addr     = gethostbyname(argv[1]);
    client_info->port            = atoi(argv[2]);
    client_info->connection_type = argv[3];
    client_info->file_name       = argv[4];
    

    fprintf(debg_ofp, "Parsed command line arguments\n");
    fprintf(debg_ofp, "Server Address: %s\t",
	    client_info->server_addr->h_name);
    fprintf(debg_ofp, "Port No: %d\t", client_info->port);
    fprintf(debg_ofp, "Connection Type: %s\t",
	    client_info->connection_type);
    fprintf(debg_ofp, "File Name: %s\t\n", client_info->file_name);
    return 0;
    
}

int bootstrap_client(int *client_sock_fd, client_info_t client_info,
		     struct sockaddr_in *server_addr, FILE *debg_ofp)
{
    int                status;

    *client_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
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

int is_persistent(client_info_t req_info)
{
    if (strncmp(req_info.connection_type, "np",
		strlen(req_info.connection_type)) == 0) {
	return 1;
    }
    else
    {
	return 0;
    }
}

int build_http_get_request(client_info_t req_info,
			    char *resp_buf, FILE *debg_ofp)
{
    int    http_version;
 
    http_version = is_persistent(req_info);
    sprintf(resp_buf, "GET\t/%s\t%s\r\n", req_info.file_name,
	    (http_version == 0)?"HTTP/1.1":"HTTP/1.0");
    sprintf(resp_buf, "%sHost: %s:%d\r\n", resp_buf,
	    req_info.server_addr->h_name, req_info.port);
    sprintf(resp_buf, "%sUser-Agent: %s\r\n", resp_buf, "Lannister");
    sprintf(resp_buf, "%sAccept: %s\r\n", resp_buf, "text/html");
    sprintf(resp_buf, "%sAccept-Language: %s\r\n", resp_buf, "en-US,en;q=0.5");
    sprintf(resp_buf, "%sAccept-Encoding: %s\r\n", resp_buf, "gzip, deflate");
    sprintf(resp_buf, "%sConnection: %s\r\n", resp_buf, "keep-alive");

    fprintf(debg_ofp, "INFO: GET Response: \n %s\n", resp_buf);
    return 0;
}

int main(int argc, char *argv[])
{

    int                status;
    client_info_t      client_info;
    char               file_name[] = "web_client_trace.log";
    struct sockaddr_in server_addr;
    char               buf[8192];

    debg_ofp = fopen(file_name, "w");
    status = parse_cmd_line_args(argc, argv, &client_info, debg_ofp);
    if(status != 0) {
	fprintf(stderr, "ERROR: Invalid arguments check log\n");
	cleanup(client_sock_fd, debg_ofp);
	exit(1);
    }
    status = bootstrap_client(&client_sock_fd, client_info,
			      &server_addr, debg_ofp);
    if(status != 0) {
	fprintf(stderr, "ERROR: Creating a socket\n");
	fprintf(debg_ofp, "ERROR: Creating a socket\n");
	exit(1);
    }
   
    status = connect(client_sock_fd, (struct sockaddr*)&server_addr,
		     sizeof(server_addr)); 
    if (status < 0) {
	fprintf(stderr, "ERROR: Connecting to server\n");
	fprintf(debg_ofp, "ERROR: Connecting to server\n");
	exit(1);
    }
    status = build_http_get_request(client_info, buf, debg_ofp);
    status = write(client_sock_fd, buf, strlen(buf));
    if (status < 0) {
	fprintf(stderr, "Error writing into the socket\n");
	fprintf(debg_ofp, "Error writing into the socket\n");
	exit(1);
    }
    bzero(buf, 8192);
    status = read(client_sock_fd, buf, 8192);
    if (status < 0) {
	fprintf(stderr, "Error reading from the socket\n");
	fprintf(debg_ofp, "Error reading into the socket\n");
	exit(1);
    }
    fprintf(stdout, "Incoming Message: \n %s", buf);
    fprintf(debg_ofp, "Incoming Message: \n %s", buf);
    cleanup(client_sock_fd, debg_ofp);
    return 0;
}



