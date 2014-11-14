/*
 * =====================================================================================
 *
 *       Filename:  probectl.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年11月14日 14时59分06秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/socket.h>

static int connect_to_server(char *sock_name)
{
    int sock;
    struct sockaddr_un  sa_un;
                                                                                                                     
    /* Connect to socket */
    sock = socket(AF_UNIX, SOCK_STREAM, 0); 
    memset(&sa_un, 0, sizeof(sa_un));
    sa_un.sun_family = AF_UNIX;
    strncpy(sa_un.sun_path, sock_name, (sizeof(sa_un.sun_path) - 1));

    if (connect(sock, (struct sockaddr *)&sa_un, 
            strlen(sa_un.sun_path) + sizeof(sa_un.sun_family))) {
        fprintf(stderr, "ctl: ismart_probe probably not started (Error: %s)\n", strerror(errno));
        exit(1);
    }   

    return sock;
}

static size_t send_request(int sock, char *request)
{   
    size_t  len;
        ssize_t written;                             
                                                     
    len = 0;
    while (len != strlen(request)) {
        written = write(sock, (request + len), strlen(request) - len);
        if (written == -1) {         
            fprintf(stderr, "Write to ismart_probe failed: %s\n",
                    strerror(errno));      
            exit(1);
        }
        len += written;
    }           

    return len;
}


int main(int argc, char **argv)
{
	int sock;
    char request[16];
	if (argc < 2)
	{
		printf("Too few para\n");
		exit(1);
	}
    sock = connect_to_server("/tmp/probectl.sock");

    strncpy(request, argv[1], 15);

    send_request(sock, request);
    shutdown(sock, 2);
    close(sock);
	return 0;
}
