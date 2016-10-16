/*
 * httpserv.c
 *
 *  Created on: 2016/10/16
 *      Author: dan
 */

#include<sys/fcntl.h>
#include<sys/socket.h>
#include<sys/types.h>

#include<netinet/in.h>
#include<netdb.h>

#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sysexits.h>
#include<unistd.h>

#define HOST "127.0.0.1"
#define PORT "80"

#define HTTP_BUF_LEN 65535
#define METHOD_NAME_LEN 16
#define URI_ADDR_LEN 256
#define HTTP_VER_LEN 64

int server_socket(const char *host, const char *port);

void accept_loop(int sock);

void http(int sockfd);

int send_msg(int fd, char *msg);

int main(int argc,char **argv) 
{
	int sock;
	
	if(argc!=1)
	{
		(void)fprintf(stderr,"Usage: %s\n",argv[0]);
		exit(EX_USAGE);
	}

	if((sock=server_socket(HOST,PORT))==-1)
	{
		(void)fprintf(stderr,"%s : fail server_socket()",argv[0]);
		exit(EX_UNAVAILABLE);
	}
	
	accept_loop(sock);

	close(sock);
	exit (EX_OK);
}


int server_socket(const char *host, const char *port) 
{
	struct addrinfo hints, *res0;
	int sock, opt, errcode;
	socklen_t optlen;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	(void) memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((errcode = getaddrinfo(host, port, &hints, &res0)) != 0) 
	{
		(void) fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(errcode));
		return (-1);
	}

	if ((errcode = getnameinfo(res0->ai_addr, res0->ai_addrlen, 
							  hbuf, (socklen_t)sizeof(hbuf),
							  sbuf, (socklen_t)sizeof(sbuf),
							  NI_NUMERICHOST | NI_NUMERICSERV)) != 0) 
	{
		(void) fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(errcode));
		freeaddrinfo(res0);
		return (-1);
	}

	if ((sock = socket(res0->ai_family, res0->ai_socktype, res0->ai_protocol))
			== -1) 
	{
		perror("socket");
		freeaddrinfo(res0);
		return (-1);
	}

	opt = 1;
	optlen = sizeof(opt);

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, optlen) != 0) 
	{
		perror("setsockopt");
		freeaddrinfo(res0);
		close(sock);
		return (-1);
	}

	if (bind(sock, res0->ai_addr, res0->ai_addrlen) == -1) 
	{
		perror("bind");
		freeaddrinfo(res0);
		close(sock);
		return (-1);
	}

	if (listen(sock, SOMAXCONN) == -1) 
	{
		perror("listen");
		freeaddrinfo(res0);
		close(sock);
		return (-1);
	}

	/* this addrinfo is not necessary anymore */
	freeaddrinfo(res0);
	
	return (sock);
}


void accept_loop(int sock) 
{
	int acc, errcode;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	struct sockaddr_storage from;
	socklen_t len = sizeof(from);

	for (;;) 
	{
		acc = 0;
		
		if ((acc = accept(sock, (struct sockaddr*) &from, &len)) == -1) 
		{
			perror("accept");
			/* when SIGINT raise this error, continue this loop */
			if (errno == EINTR) 
			{
				(void) fprintf(stderr, "accept error: EINTR\n");
				continue;
			}
		} 
		else 
		{
			if ((errcode = getnameinfo((const struct sockaddr*)&from, len, hbuf, sizeof(hbuf), sbuf,
					sizeof(sbuf),
					NI_NUMERICHOST | NI_NUMERICSERV)) == -1)
			{
				(void) fprintf(stderr, "getnameinfo():%s\n",gai_strerror(errcode));
			}
			
			(void) fprintf(stderr, "accept from %s : %s\n", hbuf, sbuf);

			/* http application */
			http(acc);

			close(acc);
		}
	}
}


void http(int sockfd) 
{
	int len, read_fd;
	char buf[HTTP_BUF_LEN];
	char method_name[METHOD_NAME_LEN];
	char uri_addr[URI_ADDR_LEN];
	char http_ver[HTTP_VER_LEN];
	char *uri_file;

	if (recv(sockfd, buf, HTTP_BUF_LEN, 0) <= 0) 
	{
		fprintf(stderr, "error: reading a request.\n");
		return;
	} 
	else 
	{
		sscanf(buf, "%s %s %s", method_name, uri_addr, http_ver);
		if (strcmp(method_name, "GET") != 0) 
		{
			send_msg(sockfd, "501 Not Implemented");
			return;
		} 
		else 
		{
			uri_file = uri_addr + 1;
			if(strcmp(uri_file,"")==0)
			{
				uri_file="index.html";
			}

			if ((read_fd = open(uri_file, O_RDONLY, 0666)) == -1) 
			{
				send_msg(sockfd, "404 Not Found");
				return;
			} 
			else 
			{
				send_msg(sockfd, "HTTP/1.1 200 OK\r\n");
				send_msg(sockfd, "Content-Type: text/html\r\n");
				send_msg(sockfd, "\r\n");
				while ((len = read(read_fd, buf, HTTP_BUF_LEN)) > 0) 
				{
					if (write(sockfd, buf, len) != len) 
					{
						fprintf(stderr, "error: writing http response.\n");
						break;
					}
				}
				close(read_fd);
				return;
			}
		}
	}
}


int send_msg(int fd, char *msg)
{
	int len;
	
	len=strlen(msg);
	if(write(fd,msg,len)!=len)
	{
		(void)fprintf(stderr,"error: send_msg()\n");
		return (-1);
	}
	return len;
}
