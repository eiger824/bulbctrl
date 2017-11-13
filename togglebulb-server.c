/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
/*
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char **argv)
{
    int parentfd; /* parent socket */
    int childfd; /* child socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
    unsigned char buf[BUFSIZE]; /* message buffer */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */

    /* 
     * check command line arguments 
     */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /* 
     * socket: create the parent socket 
     */
    parentfd = socket(AF_INET, SOCK_STREAM, 0);
    if (parentfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));

    /* this is an Internet address */
    serveraddr.sin_family = AF_INET;

    /* let the system figure out our IP address */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* this is the port we will listen on */
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(parentfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * listen: make this socket ready to accept connection requests 
     */
    if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
        error("ERROR on listen");

    /* 
     * main loop: wait for a connection request, echo input line, 
     * then close connection.
     */
    clientlen = sizeof(clientaddr);
    while (1) {

        /* 
         * accept: wait for a connection request 
         */
        childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
        if (childfd < 0) 
            error("ERROR on accept");

        /* 
         * gethostbyaddr: determine who sent the message 
         */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
                sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        printf("server established connection with %s (%s)\n", 
                hostp->h_name, hostaddrp);

        /* 
         * read: read input string from the client
         */
        bzero(buf, BUFSIZE);
        n = read(childfd, buf, BUFSIZE);
        if (n < 0) 
            error("ERROR reading from socket");
        printf("server received %d bytes\n", n);
        buf[n] = '\0';

        // Analyze buffer
        if (buf[0] != 0x01)
        {
            fprintf(stderr, "Wrong client ID: %d\n", (int)buf[0]);
        }
        else
        {
            if (buf[1] > 0x03)
            {
                fprintf(stderr, "Wrong host ID: %d\n", (int)buf[1] + 48);
            }
            else
            {
                int fd, err;
                fd = open("/dev/bulbctrl", O_RDWR);
		printf("fd was: %d\n", fd);
                char rsp[4];
                rsp[0] = buf[0];
                rsp[1] = buf[1];
                if (buf[2] == 0x1a) //Get
                {
			printf("GET request received\n");
                    // Write it to the client
                    if (fd < 0)
                    {
                        fprintf(stderr, "Error opening device\n");
                        rsp[2] = 0x7f; //Fail
                    }
                    else
                    {
                        char current_value[2];
                        if ((err = read(fd, current_value, 1)) < 0)
                        {
                            fprintf(stderr, "Error reading from device\n");
                            rsp[2] = 0x7f; //Fail
                        }
                        else
                        {
                            current_value[1] = 0; 
                            rsp[2] = current_value[0];
				printf("Current value turns out to be: %s\n", current_value);
                        }
                    }
                    rsp[3] = '\0';
                    n = write(childfd, rsp, 3);
			printf("Wrote %zu bytes back to client\n", n);
                    close(fd);
                }
                else if (buf[2] == 0x7f) //set
                {
                    if (fd < 0)
                    {
                        fprintf(stderr, "Error opening device\n");
                        rsp[2] = 0x7f; //Fail
                    }
                    else
                    {
                        if (buf[3] != 0x00 && buf[3] != 0x7f) 
                        {
                            fprintf(stderr, "Wrong value to set\n");
                            rsp[2] = 0x7f;
                        }
                        else
                        {
                            char current[2];
                            if (( err = read(fd, current, 1)) < 0)
                            {
                                fprintf(stderr, "Error reading from device\n");
                                rsp[2] = 0x7f;
                            }
                            else
                            {
                                int c = atoi(current);
                                if (buf[3] == 0x00)
                                {
                                    if (c)
                                    {
                                        printf("Swtiching OFF!\n");
                                        err = write(fd, "0", 1);
                                    }
                                }
                                else
                                {
                                    if (!c)
                                    {
                                        printf("Switching ON!\n");
                                        err = write(fd, "1", 1);
                                    }
                                }
                            }
                        }
                        close(fd);
                    }
                }
            }
        }

        /* 
         * write: echo the input string back to the client 
         */
        n = write(childfd, buf, strlen(buf));
        if (n < 0) 
            error("ERROR writing to socket");

        close(childfd);
    }
}
