#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char default_root[] = ".";

//
// Some of this code stolen from Bryant/O'Halloran
// Hopefully this is not a problem ... :)
//

#define MAXBUF (8192)

/* this is not the standard readline function. */
ssize_t readline(int fd, void *buf, size_t maxlen) {
    char c;
    char *bufp = buf;
    int n;
    for (n = 0; n < maxlen - 1; n++) { // leave room at end for '\0'
	int rc;
	if ((rc = read(fd, &c, 1)) == 1) {
	    *bufp++ = c;
	    if (c == '\n')
		break;
	} else if (rc == 0) {
	    if (n == 1)
		return 0; /* EOF, no data read */
	    else
		break;    /* EOF, some data was read */
	} else
	    return -1;    /* error */
    }
    *bufp = '\0';
    return n;
}

/* request handle functions */

void request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXBUF], body[MAXBUF];
    
    // Create the body of error message first (have to know its length for header)
    snprintf(body, MAXBUF, ""
	    "<!doctype html>\r\n"
	    "<head>\r\n"
	    "  <title>WebServer Error</title>\r\n"
	    "</head>\r\n"
	    "<body>\r\n"
	    "  <h2>%s: %s</h2>\r\n" 
	    "  <p>%s: %s</p>\r\n"
	    "</body>\r\n"
	    "</html>\r\n", errnum, shortmsg, longmsg, cause);
    
    // Write out the header information for this response
    snprintf(buf, MAXBUF, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    write(fd, buf, strlen(buf));
    
    snprintf(buf, MAXBUF, "Content-Type: text/html\r\n");
    write(fd, buf, strlen(buf));
    
    snprintf(buf, MAXBUF, "Content-Length: %lu\r\n\r\n", strlen(body));
    write(fd, buf, strlen(buf));
    
    // Write out the body last
    write(fd, body, strlen(body));
}

//
// Reads and discards everything up to an empty text line
//
void request_read_headers(int fd) {
    char buf[MAXBUF];
    
    readline(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n")) {
	readline(fd, buf, MAXBUF);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
int request_parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;
    
    if (!strstr(uri, "cgi")) { 
	// static
	strcpy(cgiargs, "");
	snprintf(filename, MAXBUF, ".%s", uri);
	if (uri[strlen(uri)-1] == '/') {
	    strcat(filename, "index.html");
	}
	return 1;
    } else { 
	// dynamic
	ptr = index(uri, '?');
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	} else {
	    strcpy(cgiargs, "");
	}
	snprintf(filename, MAXBUF, ".%s", uri);
	return 0;
    }
}

//
// Fills in the filetype given the filename
//
void request_get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html")) 
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif")) 
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg")) 
	strcpy(filetype, "image/jpeg");
    else 
	strcpy(filetype, "text/plain");
}

void request_serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXBUF], *argv[] = { NULL };
    
    // The server does only a little bit of the header.  
    // The CGI script has to finish writing out the header.
    snprintf(buf, MAXBUF, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: WebServer\r\n");
    
    write(fd, buf, strlen(buf));
    
    if (fork() == 0) {                        // child
	setenv("QUERY_STRING", cgiargs, 1);   // args to cgi go here
	dup2(fd, STDOUT_FILENO);              // make cgi writes go to socket (not screen)
	extern char **environ;                       // defined by libc 
	execve(filename, argv, environ);
    } else {
	wait(NULL);
    }
}

void request_serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[15], buf[MAXBUF];
    
    request_get_filetype(filename, filetype);
    srcfd = open(filename, O_RDONLY, 0);
    
    // Rather than call read() to read the file into memory, 
    // which would require that we allocate a buffer, we memory-map the file
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);
    
    // put together response
    snprintf(buf, MAXBUF, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: WebServer\r\n"
	    "Content-Length: %d\r\n"
	    "Content-Type: %s\r\n\r\n", 
	    filesize, filetype);
    
    write(fd, buf, strlen(buf));
    
    //  Writes out to the client socket the memory-mapped file 
    write(fd, srcp, filesize);
    munmap(srcp, filesize);
}

// handle a request
void request_handle(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[15];
    char filename[MAXBUF], cgiargs[MAXBUF];
    
	/* FIXME: somehow this program won't work if we remove this below printf statement */
	printf("request handle: ");
    /* read the first line */
    readline(fd, buf, MAXBUF);
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("method:%s uri:%s version:%s\n", method, uri, version);
    
    /* we only support get requests */
    if (strcasecmp(method, "GET")) {
	request_error(fd, method, "501", "Not Implemented", "server does not implement this method");
	return;
    }
    request_read_headers(fd);
    
    is_static = request_parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
	request_error(fd, filename, "404", "Not found", "server could not find this file");
	return;
    }
    
    if (is_static) {
	/* the client is requesting a static page, let's check permission first */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
	    request_error(fd, filename, "403", "Forbidden", "server could not read this file");
	    return;
	}
	/* if permission allowed, serve the file */
	request_serve_static(fd, filename, sbuf.st_size);
    } else {
	/* the client is requesting a dynamic page, let's check permission first */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
	    request_error(fd, filename, "403", "Forbidden", "server could not run this CGI program");
	    return;
	}
	/* if permission allowed, serve the file */
	request_serve_dynamic(fd, filename, cgiargs);
    }
}

//
// ./wserver [-d <basedir>] [-p <portnum>] 
// 
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
    
    while ((c = getopt(argc, argv, "d:p:")) != -1)
	switch (c) {
	case 'd':
	    root_dir = optarg;
	    break;
	case 'p':
	    port = atoi(optarg);
	    break;
	default:
	    fprintf(stderr, "usage: wserver [-d basedir] [-p port]\n");
	    exit(1);
	}

    // run out of this directory
    chdir(root_dir);
	
	int sockfd, newsockfd;
	struct sockaddr_in server_addr, client_addr;

	/* create a socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	/* bind to a port number */
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	bind(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));

	/* listen for connections */
	listen(sockfd, 1024);

    while (1) {
		/* accept a connection request */
		int client_len = sizeof(client_addr);
		newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t *) &client_len);

		/* read data from the connection */
		request_handle(newsockfd);

		/* close the connection */
		close(newsockfd);
	}
	close(sockfd);

	return 0;
}

/* vim: set ts=4: */
