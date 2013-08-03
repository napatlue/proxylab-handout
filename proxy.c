/*
 * Author: Napat Luevisadpaibul
 * Andrew Id: nluevisa
 */

#include <stdio.h>
#include "csapp.h"
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

#define MIN(x, y) ((x) < (y)? (x) : (y))

/* Global variable */
static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_str = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";

static const char *version = "HTTP/1.0";
sem_t mutex;


/* My function prototypes */
//void doit(int, struct Cache*);
void doit(int clientfd);                          // function to interact with server and client
void clienterror(int, char*, char*, char*, char*);
void get_url_info(char* url, char* p_port, char* host, char* uri); //parse request from client in url form
//void parse_build_requesthead(rio_t*, char*);
void* thread(void* vargp);
struct hostent *ts_gethostbyname(const char* name);  // thread-safe version of gethostbyname

int ts_open_serverfd(char *hostname, int port, int fd);
void build_header(rio_t* rp, char* header, char* hostname);

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
int Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
int forwardResponseHeader(rio_t *rio_server, int clientfd, int *statusCode, int *contentLength);
int forwardPayload(rio_t *rio_server, int clientfd, int contentLength);


int forwardResponseHeader(rio_t *rio_server, int clientfd, int *statusCode, int *contentLength){
	char buf[MAXLINE];
	int n;
    
	strcpy(buf, "\0"); // Initialize buffer
    
	while ( strcmp(buf, "\r\n") != 0)	{
		if ( (n = Rio_readlineb_w(rio_server, buf, MAXLINE)) == 0)
			return 1;
		Rio_writen_w(clientfd, buf, n);
		Fputs(buf, stdout);
        
		// Parse status code and content length
		sscanf(buf, "HTTP/1.1 %d", statusCode);
		sscanf(buf, "Content-Length: %d", contentLength);
	}
    
	return 1;
}


int forwardPayload(rio_t *rio_server, int clientfd, int contentLength){
	char buf[MAXLINE];
	int n;
    
	if (contentLength > 0 ){

		while ( (n = Rio_readnb_w(rio_server, buf, MIN(MAXLINE, contentLength))) > 0 ){
			Rio_writen_w(clientfd, buf, n);
			contentLength -= n;
		}
	}
	else {
        fprintf(stderr, "Empty payload\n");
	}
	return 1;
}

/*
 * create request header from client request 
 *
 */
void build_header(rio_t* rp, char* header, char* hostname)
{
	char buf[MAXLINE];
	Rio_readlineb(rp, buf, MAXLINE);
	int has_host=0,has_ua = 0, has_ac = 0, has_ae = 0, has_co = 0, has_pc = 0;
    
	while(strcmp(buf, "\r\n") && strcmp(buf, "\n"))
	{
        if(strstr(buf, "Host:")){
            sprintf(header, "%s%s", header,buf);
            has_host=1;
        }
        else if(strstr(buf, "User-Agent:")){
			sprintf(header, "%s%s",header,user_agent);
			has_ua = 1;
		}
		else if(strstr(buf, "Accept:")){
			sprintf(header, "%s%s",header,accept_str);
			has_ac = 1;
		}
		else if(strstr(buf, "Accept-Encoding:")){
			sprintf(header, "%s%s",header,accept_encoding);
			has_ae = 1;
		}
		else if(strstr(buf, "Connection:")){
			sprintf(header, "%sConnection: close\r\n",header);
			has_co = 1;
		}
		else if(strstr(buf, "Proxy-Connection:")){
			sprintf(header, "%sProxy-Connection: close\r\n",header);
			has_pc = 1;
		}
		else
		{
			sprintf(header, "%s%s", header,buf);
		}
		Rio_readlineb(rp, buf, MAXLINE);
	}
    
    if(!has_host){
        sprintf(header, "%sHost: %s\r\n", header,hostname);
    }
    
	if(!has_ua){
		sprintf(header, "%s%s",header,user_agent);
	}
    
	if(!has_ac){
		sprintf(header, "%s%s",header,accept_str);
	}
    
	if(!has_ae){
		sprintf(header, "%s%s",header,accept_encoding);
	}
	if(!has_co){
		sprintf(header, "%sConnection: close\r\n",header);
	}
    
	if(!has_pc){
		sprintf(header, "%sProxy-Connection: close\r\n",header);
	}
	sprintf(header, "%s\r\n", header);
}


int ts_open_serverfd(char *hostname, int port, int fd)
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;
    dbg_printf("Openclient: %s:%d fd=%d\n",hostname,port,fd);
    
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
		fprintf(stderr, "ts_open_clientfd error: %s\n",strerror(errno));
        return -1; 
    }
  
    if ((hp = ts_gethostbyname(hostname)) == NULL)
    {
		clienterror(fd, hostname, "Error ", "ts_gethostbyname error", "Invalid hostname ");
        return -1;
    }
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],
          (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);
	Free(hp);

    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
    {
		fprintf(stderr, "ts_open_clientfd error: %s\n",strerror(errno));
        return -1;
    }
    return clientfd;
}

void* thread(void* vargp)
{
	
    Pthread_detach(Pthread_self());
	int connfd = (*(int*)vargp);
	//Cache* p_Cache = (*(struct main_args*)arg).p_Cache;
	dbg_printf("Spawn thread with connfd: %d\n",connfd);
    
    Free(vargp);
	//doit(connfd, p_Cache);
	doit(connfd);
    Close(connfd);
	return NULL;
}
/*  This funtion get request from client and forward to server and also forward response from server to client
 *  Note: if proxy recieve "GET http://www.cmu.edu:8080/hub/index.html HTTP/1.1",
 *  it should send:        "GET /hub/index.html HTTP/1.0  to server's 8080 port"
 */
void doit(int clientfd)
{
	int cache_hit = 0;
    
    rio_t rio_client, rio_server;
	
	char buf[MAXLINE], method[10], url[MAXLINE], uri[MAXLINE];
    char hostname[MAXLINE];
    //char response[MAXBUF];

	char request_line[MAXLINE] = ""; //request_line built by proxy that will be sent to server
	char request_header[MAXLINE] = ""; // header bulit by proxy that will be sent to server
	char request_port[10] = ""; // client assigns a port
	//char version[] = "HTTP/1.0";
	//char save_buf[MAX_OBJECT_SIZE + 1] = ""; // save temp_data from server, no bigger than MAX_OBJ_SIZE
	int serverfd, port;
	//int read_size;
	int statusCode, contentLength;

	//Cache* p_is_hit; // if hit cache, this pointer holds the addrress of cache_block; if not hit, return NULL
	
    /* Read request from client */
    Rio_readinitb(&rio_client, clientfd);
	/*
    if(rio_readlineb(&rio_client, buf, MAXLINE) < 0)
	{
		fprintf(stderr, "rio_readlineb error: %s\n",strerror(errno));
		return;
	}
     */
    if ( (Rio_readlineb_w(&rio_client, buf, MAXLINE)) == 0)
    {
		return;
    }
	sscanf(buf, "%s %s",method, url);
    //check client request
	if(strcasecmp(method, "GET"))
	{
		clienterror(clientfd,method, "501", "Not Support","Proxy doesn't support this request type");
		Close(clientfd);
		Pthread_exit(NULL);
	}
	
    get_url_info(url, request_port, hostname, uri);
	
    port = !strlen(request_port) ? 80 : atoi(request_port);
	sprintf(url,"%s:%d%s", hostname, port, uri); // rebuild url, using url as a key to search cache
    dbg_printf("URL:%s \n",url);
    //fprintf(stderr, "url: %s\n",url);
	//if( Search_and_Transfer(url, p_Cache,fd,(void**)&p_is_hit) < 0)
	//	return;
	if(!cache_hit) // if not hit cache, proxy connect to server, receive and build cache block
	{
		fprintf(stderr ,"miss\n");
		sprintf(request_line, "%s %s %s\r\n", method, uri, version); // build request line
		build_header(&rio_client, request_header, hostname);
        
        fprintf(stderr ,"Request line %s\n",request_line);
        fprintf(stderr ,"Header %s\n",request_header);
        if((serverfd = ts_open_serverfd(hostname, port, clientfd)) < 0)
        {
			return;
        }
		//printf("%s: %d", host, port);
        
		if(rio_writen(serverfd, request_line, strlen(request_line)) < 0) // proxy send request-line to server
		{
			Close(serverfd);
			return;
		}
		if(rio_writen(serverfd, request_header, strlen(request_header)) < 0)// proxy send request-header to server
		{
			Close(serverfd);
			return;
		}
        
        dbg_printf("-------------Begin forward response header\n\n");
        
		Rio_readinitb(&rio_server, serverfd);
        
        if (!forwardResponseHeader(&rio_server, clientfd, &statusCode, &contentLength)){
            Close(serverfd);
            return;
        }
        
        dbg_printf("-------------Begin forward response payload\n\n");
        if (statusCode == 200){
            forwardPayload(&rio_server, clientfd, contentLength);
        }
        
        
		/* get server's response and save in save_buf temporarily */
        //		int total_size = 0;
		/*
        if((read_size = rio_readnb(&rio, response, MAXBUF)) > 0)
		{
			dbg_printf("response: %s\n",response);
            
            if(read_size <= MAX_OBJECT_SIZE) // object should be cached and send back to client
			{
				fprintf(stderr, "cache!\n");
				if(rio_writen(clientfd, save_buf, read_size) < 0) // sent back to client
				{
                    Close(serverfd);
					return;
				}
                dbg_printf("save buffer: %s\n",save_buf);
				//Write_and_Update(p_Cache, read_size, url, save_buf); // write to cache and update access time
			}
			else // should not cache, transfer it to client directly
			{
				fprintf(stderr, "not cache!\n");
				if(rio_writen(clientfd, save_buf, read_size) < 0)
				{
					Close(serverfd);
					return;
				}
				while((read_size = rio_readnb(&rio, save_buf, MAX_OBJECT_SIZE+1)) > 0)
                {
					if(rio_writen(clientfd, save_buf, read_size) < 0)
					{
                        break;
                    }
                    dbg_printf("save buffer: %s\n",save_buf);
                }
			}
		}
         */
		Close(serverfd);
	}

    
}


void get_url_info(char* url, char* p_port, char* host, char* uri)
{
	char* ptr, *p;
	if((ptr = strstr(url, "://")) == NULL) // if brower doesn't append http:// head, append it manually
	{
		char temp[MAXLINE];
		strcpy(temp, "http://");
		strcat(temp, url);
		strcpy(url, temp);
		ptr = url + 4;
	}
	ptr += 3; // cut http:// from request line
	url = ptr;
	if((ptr = strstr(url, "/")) == NULL) // if brower doesn't append '/', then append it manually
	{
		size_t len = strlen(url);
		url[len] = '/';
		url[len+1] = '\0';
		ptr = url + len;
	}
	p = ptr;
	while(*(p + 1) == '/'){
		p += 1;
	}
	strcpy(uri, p); // construct uri
	*ptr = '\0';
	if((ptr = strstr(url, ":")) != NULL) // if client provide a port
	{
		*ptr = '\0';
		strcpy(p_port, ptr+1);
	}
	strcpy(host, url); // get Host: www.cmu.edu
}

int main(int argc, char* argv[])
{
    //printf("%s%s%s", user_agent, accept, accept_encoding);
    struct sockaddr_in clientaddr;
    int listenfd,port;
	socklen_t clientlen;
	int *connfdp;
    
    pthread_t tid;
    
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <port>\n",argv[0]);
		return 1;
	}
    
	// Catch SIGPIPE Signal
    Signal(SIGPIPE, SIG_IGN);
 
    
	/* construct and initialize header of cache linked list */
    
	/*
    Cache* p_Cache = Malloc(sizeof(struct Cache));
	p_Cache->obj_size = 0;
	p_Cache->last_access = 0;
	p_Cache->p_url = NULL;
	p_Cache->p_next = NULL;
	p_Cache->response_body = NULL;
    */
    
	Sem_init(&mutex,0,1); // use binary semaphore
	/*
    Sem_init(&w_mutex,0,1);
	Sem_init(&cnt_mutex,0,1);
	Sem_init(&mtime_mutex,0,1);
    */
	port = atoi(argv[1]);
	dbg_printf("Port: %d\n",port);
    clientlen = sizeof(clientaddr);
    
	listenfd = Open_listenfd(port);
    dbg_printf("listenfd: %d\n",listenfd);
	//main_args* p_arg;
	while(1)
	{
		//p_arg = Malloc(sizeof(struct main_args));
		//(*p_arg).p_Cache = p_Cache;
		connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA*)&clientaddr, &clientlen);
		Pthread_create(&tid, NULL, thread, connfdp);
	}
    
    return 0;
}

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Proxy Server</em>\r\n", body);
    
    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

// thread safe version of gethostbyname
struct hostent *ts_gethostbyname(const char* name)
{
    struct hostent *p;
	struct hostent *result = Malloc(sizeof(struct hostent));
	P(&mutex);
    if ((p = gethostbyname(name)) == NULL)
	{
		V(&mutex);
		Free(result);
		return NULL;
	}

	*result = *p;
	V(&mutex);
    return result;
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
	ssize_t rc;
    
	if (( rc = rio_readlineb(rp, usrbuf, maxlen )) < 0)
	{
		printf("Rio_readlineb_w error!\n");
		return 0; // Return EOF if error
	}
    
	return rc;
}

int Rio_writen_w(int fd, void *usrbuf, size_t n)
{
	if (rio_writen(fd, usrbuf, n) != n)
	{
		printf("Rio_writen_w error!\n");
		return 0;
	}
    
	return 1;
}

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n_size)
{
	ssize_t rc;
    
	if ( (rc = rio_readnb(rp, usrbuf, n_size)) < 0)
	{
		printf("Rio_readnb_w error!\n");
		return 0;
	}
    
	return rc;
}

