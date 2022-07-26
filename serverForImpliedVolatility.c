
#include <sys/socket.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/wait.h>      // declare functions such as waitpid and wait
#include <sys/types.h>   
#include <netinet/in.h>   // define data types such as sockaddr_in structure
#include <arpa/inet.h>  
#include <unistd.h>     // declare functions related with process manipulations like dup2, fork, execl, pipe and so on
#include <ctype.h>
#include <strings.h>
#include <string.h>  
#include <sys/stat.h>
#include <stdlib.h>
#include <regex.h>   // for regular expression related stuff


#define SERVER_STRING "Server: server_for_implied_volatility/1.0\r\n"
#define ISspace(x) isspace((int)(x))



void* accept_request(void*);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
int match(char*, char*);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);
void wrong_format(int);



void* accept_request (void* client)  {
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;          
    struct stat st;    
    int cgi = 0;       // cgi signal will take 1 if executing cgi is required
    char *query_string = NULL;
    char pattern[255];

    numchars = get_line(*((int*) client), buf, sizeof(buf));
    i = 0; j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))  {     // three parts of a request line are seprated by space
        method[i] = buf[j];    // the method token comes first in a request line
        i++; j++;
    }
    method[i] = '\0';   // mark the end of a string

    if (strcasecmp(method, "GET") != 0)   {       //function strcasecmp compares two strings without sensitivity to case
        unimplemented(*((int*) client));
        return NULL;
    }

    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;                                        // skip spaces
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))  {
        url[i] = buf[j];                           // the url comes the second in a request line
        i++; j++;
    }
    url[i] = '\0';      

    query_string = url;        
    while ((*query_string != '?') && (*query_string != '\0'))
        query_string++;
    if (*query_string == '?')  {
        *query_string = '\0';
        query_string++;      // the query_string is just behind the address of question mark in an url
    }
 
   
    sprintf(path, "/server_for_implied_volatility%s", url);         // sprintf sends formatted output to the string pointed to by path
    if (path[strlen(path) - 1] == '/')                             // if url takes /, just lead to the index page with default
        strcat(path, "index.html");                               // strcat contcatenates two strings

    char non_negative[] = "(0[.][0-9]{1,3}|[1-9][0-9]*([.][0-9]{1,3})?)";     // the regular espression of non-negative number with up to 3 decimals
    sprintf(pattern, "^type=[01]&s=%s&k=%s&t=%s&r=%s&p=%s$", non_negative, non_negative, non_negative, non_negative, non_negative);

    if (strcasecmp(path, "/server_for_implied_volatility/index.html") == 0)
        serve_file(*((int*) client), path);
    else if (match(query_string, pattern) == 0)                           //if a query_string conforms the pattern used by implied_volatility cgi, then execute it 
        execute_cgi(*((int*) client), path, query_string);
    else  {
        while ((numchars > 0) && strcmp("\n", buf))                      // read & discard the rest of headers, which will no longer be used. 
            numchars = get_line(*((int*) client), buf, sizeof(buf));
        wrong_format(*((int*) client));
    }

    close(*((int*) client));
    
    pthread_detach(pthread_self());   // to make sure momoey space of a thread could be released immediately once it terminates, claim thread attribute as detached
    return NULL;
}


void cat(int client, FILE *resource) {
    char buf[1024];

    fgets(buf, sizeof(buf), resource);      // if fgets reaches the end of file, then only after using fgets again, can feof recognize the end 
    while (!feof(resource)) {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}



void cannot_execute(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}



void error_die(const char *sc)  {
    perror(sc);              // perror prints a descriptive error message to stderr
    exit(1);
}



void execute_cgi(int client, const char *path, const char *query_string)  {
    char buf[1024];
    int cgi_pipe[2];
    pid_t pid;
    int status;
    int i;
    char c;                 // use it to temporarily store a character received from pipe
    int numchars = 1;

    buf[0] = 'A'; buf[1] = '\0';

    while ((numchars > 0) && strcmp("\n", buf))    
        numchars = get_line(client, buf, sizeof(buf));

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if (pipe(cgi_pipe) < 0) {
        cannot_execute(client);
        return;
    }

    if ( (pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }

    if (pid == 0) {               //child process, inheriting the thread resources which called fork, is responsible for dealing data based on CGI script
        char query_env[255];

        dup2(cgi_pipe[1], 1);               //set the write end of the pipe as the stdout 
        close(cgi_pipe[0]);                 //dsicard the read end of the pipe, which won't be used in child process
         
        sprintf(query_env, "QUERY_STRING=%s", query_string);
        putenv(query_env);                 //putenv could change or add an environment variable

        execl(path, path, NULL);          // replace the current process image with a new one by executing the file specified by path
        exit(0);                                     //terminates and sends a signal to parent
    } else {                             //parent process, responsible for collecting outputs from child process and then sending to client
        close(cgi_pipe[1]);                   // discard the write end of the pipe, which won't be used in parent process
        while (read(cgi_pipe[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        close(cgi_pipe[0]);
        waitpid(pid, &status, 0);       // should block until receiving a signal from the specific child process, until then the momory space of that child could be cleaned up by the kernel 
    }
}



/*
Get a line from a socket, whether the line ends in a newline, carriage return, or a CRLF combination.  
Terminates the string read with a null character.  
If no newline indicator is found before the end of the buffer, the string is terminated with a null.  
If any of the above three line terminators is read, the last character of the string will be a linefeed and the string will be terminated with a 
null character.
*/

int get_line(int sock, char *buf, int size)  {                  // buf points to the buffer that save data in; size claims the size of the buffer
    int i = 0;
    char c = '\0';                                                          // use variable c to temporarily store 1 byte of data 
    int n;

    while ((i < size - 1) && (c != '\n'))  {
        n = recv(sock, &c, 1, 0);      // 1 indicates that the length in bytes of the buffer c is 1. function recv returns the length of the message written to the buffer  
        if (n > 0)  {
            if (c == '\r')  {
                n = recv(sock, &c, 1, MSG_PEEK);         //set the MSG_PEEK flag to peek at the incoming message, which will be treated as the read target by the next recv() function
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';
 
    return(i);
}




void headers(int client, const char *filename) {         // deliver the informational HTTP headers
    char buf[1024];
    (void)filename;  

    strcpy(buf, "HTTP/1.0 200 OK\r\n");                 // strcpy copies the string pointed to by source to the destination, overwriting is implemented
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}



int match(char *string, char *pattern)  {
    int    status;
    regex_t    re;
    size_t nmatch = 1;
    regmatch_t pmatch[1];
    if (regcomp(&re, pattern, REG_EXTENDED) != 0)                 // here the extended regular expression is necessary for appropriate compiling
        return(1);                                               // report error 
    status = regexec(&re, string, nmatch, pmatch, 0);
    regfree(&re);
    if (status != 0) {
        return(1);                                             // report error 
    }
    return(0);
}




void serve_file(int client, const char *filename) {
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");               // fopen returns a FILE pointer
    headers(client, filename);
    cat(client, resource);                        // deliver the whole content of a file to a socket
    fclose(resource);
}




int startup(u_short *port) {
    int httpd;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;                      // AF_INET stands for IPv4 Internet protocols 
    name.sin_port = htons(*port);                  // converts the port number from host byte order to network byte order for the sake of internet transmission
    name.sin_addr.s_addr = htonl(INADDR_ANY);     // accepts connections to all IPs of the machine, instand of a specific ip
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    if (listen(httpd, 5) < 0)                   // here set 5 as the maximum length to which the queue of pending connections for sockfd may grow
        error_die("listen");
    return(httpd);
}



void unimplemented(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}




void wrong_format(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Wrong Format</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because your input\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is formatted incorrectly.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}





int main(void) {
	int server_sock = -1;
	u_short port = 80;
	int client_sock = -1;
	struct sockaddr_in client_name;
	int client_name_len = sizeof(client_name);
	pthread_t newthread;

	server_sock = startup(&port);
	printf("server is running on port: %d\n", port);
    while(1) {
    	client_sock = accept(server_sock, (struct sockaddr*) &client_name, &client_name_len);   //accept will block until client side have launched

    	if (client_sock == -1)
   			error_die("accept");
 	
		if (pthread_create(&newthread, NULL, &accept_request, (void*) &client_sock) != 0)     //create a thread for dealing each request 
  			perror("pthread_create");
 	}

    close(server_sock);

    return(0);
}











