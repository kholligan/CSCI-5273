// Web proxy
// Author: Kevin Holligan
// Network Systems

#include <stdio.h>
#include <string.h>    
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <unistd.h>    
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAXBUFSIZE 1024
#define LARGEBUFSIZE 4096
#define HUGEBUFSIZE 131072
#define CACHESIZE 131072

struct cache {
    char pageData[HUGEBUFSIZE];
    char url[MAXBUFSIZE];
    int pageSize; 
    time_t lastUsed;
    struct cache *next;
    // struct cache *prev;
};

typedef struct cache cache;
cache* head = NULL;
int cacheExpiration;
// typedef struct web_object * web_object;

void addPageToCache(char *data, char *url, int pageSize)
{
    //Allocate memory and assign values to struct object
    cache *page = (cache *)malloc(sizeof(page)*HUGEBUFSIZE);
    // page->pageData = (char *)malloc(HUGEBUFSIZE*sizeof(char));
    // page->url = (char *)malloc(MAXBUFSIZE*sizeof(char));
    // memset(&page->pageData, 0, HUGEBUFSIZE);
    // memset(&page->url, 0, MAXBUFSIZE);
    page->next = head;
    strcpy(page->pageData, data);
    strcpy(page->url,url);
    page->pageSize = pageSize;
    page->lastUsed = time(NULL);
    head = page;
}
// void removePageFromCache()
// {
//     cache *cur;
//     cache *prev;
//     cache *temp;

//     //Iterate through the list
//     if(head != NULL)
//     {
//         printf("in here\n");
//         // printf("cur: %s\n", cur);
//         for(cur = head, prev = head, temp = head; cur->next != NULL; cur=cur->next)
//         {
//             //If our time has exceeded the cache time remove the node from list
//             time_t now;
//             time(&now);
//             double difference = difftime(now,cur->lastUsed);
//             if(difference > cacheExpiration)
//             {
//                 //Update pointers and free memory
//                 prev->next = cur->next;
//                 temp = cur;
//                 cur = prev;
//             }
//         }
//         printf("finished for loop\n");
//     }

//     free(temp);
// }

cache* findCachePage(char * url)
{
    cache *page = NULL;
    if(head != NULL)
    {
        for(page = head; page != NULL; page = page->next)
        {
            if(strcmp(page->url, url) == 0)
            {
                return page;
            }
        }
    }
    else
    {
        // printf("Couldn't find page\n");
        return NULL;
    }

    return NULL;
}

//Adapted from http://cboard.cprogramming.com/c-programming/81901-parse-url.html
void parseHostName(char* requestURI, char* hostName)
{
    sscanf(requestURI, "http://%511[^/\n]", hostName);
}

//Read in the server's HTTP header message
int readHeader(int server_sock, char *headerBuf, int headerBufSize) {
    int i = 0;
    char c = '\0';
    int byteRec;
    //Read char by char until end of line
    while (i < (headerBufSize - 1) && (c != '\n')) 
    {
        byteRec = recv(server_sock, &c, 1, 0);
        if (byteRec > 0) 
        {
            if (c == '\r') 
            {
                //Look at the next line without reading it to evaluate
                byteRec = recv(server_sock, &c, 1, MSG_PEEK);
                //Read in the new line character if there is one
                if ((byteRec > 0) && (c == '\n')) 
                {
                    recv(server_sock, &c, 1, 0);
                }
                //Or set a new line character if there isn't
                else 
                {
                    c = '\n';
                }
            }
            headerBuf[i] = c;
            i++;
        } 
        else 
        {
            //Recv was zero or error. Set c to exit while loop
            c = '\n';
        }
    }
    headerBuf[i] = '\0';
    return i;
}

//Read in the content from server
int receiveFromServer(int server_sock, char *buf) {
    char msgBuffer[LARGEBUFSIZE];
    int contentLength = 0;
    unsigned int offset = 0;
    while (1) 
    {
        int length = readHeader(server_sock, msgBuffer, LARGEBUFSIZE);
        if( length <= 0)
        {
            return -1;
        }

        memcpy((buf + offset), msgBuffer, length);
        offset += length;
        if (strlen(msgBuffer) == 1) 
        {
            break;
        }
        if (strncmp(msgBuffer, "Content-Length", strlen("Content-Length")) == 0) 
        {
            char s1[256];
            sscanf(msgBuffer, "%*s %s", s1);
            contentLength = atoi(s1);
        }
    }
    //Read in the content from server
    char* contentBuffer = malloc((contentLength * sizeof(char)) + 3);
    int i;
    //Receive until length from header
    for (i = 0; i < contentLength; i++) 
    {
        char c;
        int byteRec = recv(server_sock, &c, 1, 0);
        if (byteRec <= 0) {
            // printf("Error in this thing\n");
            return -1;
        }
        contentBuffer[i] = c;
    }
    //Append carriage returns to end of content
    contentBuffer[i + 1] = '\r';
    contentBuffer[i + 2] = '\n';
    contentBuffer[i + 3] = '\0';
    memcpy((buf + offset), contentBuffer, (contentLength + 3));
    free(contentBuffer);
    return (offset + i + 4);
}

//Check for valid requests and output errors
int errorHandler(int client_sock, int statusCode, char* requestMethod, char* requestURI, char* requestVersion)
{
    char errorHeader[MAXBUFSIZE];
    char errorContent[MAXBUFSIZE];
    int length = 0;

    //Clear the buffers before use
    memset(&errorHeader, 0, MAXBUFSIZE);
    memset(&errorContent, 0, MAXBUFSIZE);

    //If we don't have a specific error to print (i.e. validation)
    if(statusCode == 0)
    {
        //501: not implemented
        if( (strcmp(requestMethod, "POST") == 0) || (strcmp(requestMethod, "HEAD") == 0) 
            || (strcmp(requestMethod, "PUT") ==0) || (strcmp(requestMethod, "DELETE") == 0)
            || (strcmp(requestMethod, "OPTIONS") == 0 ) || (strcmp(requestMethod, "CONNECT") == 0) )
        {
            statusCode = 501;
        }
        if ( !( (strcmp(requestVersion, "HTTP/1.1") == 0) || (strcmp(requestVersion, "HTTP/1.0") == 0) ) )
        {
            snprintf(errorContent, MAXBUFSIZE, "<html><body>400 Bad Request Reason: "
                "Invalid Version:%s</body></html>\r\n\r\n", requestVersion);

            //Create the header structure
            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 400 Bad Request\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));

            //Send header to client
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            //Write data to the client
            write(client_sock, errorContent, strlen(errorContent));

            return -1;
        }
        //400 Error: bad request method
        if( !( (strcmp(requestMethod, "GET") == 0) ) && statusCode != 501 )
        {
            snprintf(errorContent, MAXBUFSIZE, "<html><body>400 Bad Request Reason: "
                "Invalid Method:%s</body></html>\r\n\r\n", requestMethod);

            //Create the header structure
            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 400 Bad Request\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));

            //Send header to client
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            //Write data to the client
            write(client_sock, errorContent, strlen(errorContent));

            return -1;
        }
    }
    //If we want to output a specific status code
    switch(statusCode)
    {
        //File not found
        case 0:
            return 0;
            break;
        case 404:
            snprintf(errorContent, MAXBUFSIZE, "<html><body>404 Not Found "
            "Reason URL does not exist: %s</body></html>\r\n\r\n", requestURI);

            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 404 Not Found\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            
            write(client_sock, errorContent, strlen(errorContent));
            return -1;
            break;
        //Catch-all for other errors
        case 500:
            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 500 Internal Server Error\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));
            snprintf(errorContent, MAXBUFSIZE, "<html><body>500 Internal Server Error: "
                "Cannot allocate memory</body></html>\r\n\r\n");
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            
            write(client_sock, errorContent, strlen(errorContent));
            return -1;
            break;
        //Duplicate, but in case we want to call it specifically
        case 501:
            snprintf(errorContent, MAXBUFSIZE, "<html><body>501 Not Implemented "
            "Method: %s</body></html>\r\n\r\n", requestMethod);

            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 501 Not Implemented\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            
            write(client_sock, errorContent, strlen(errorContent));
            return -1;
            break;
    }

    return 0;
}

//Handles client requests
void processRequest(int client_sock, char* clientMessage, char* requestURI, char* hostName)
{
    char serverMessage[HUGEBUFSIZE];
    memset(&serverMessage, 0, HUGEBUFSIZE);

    int server_sock;
    struct sockaddr_in server;

    //Create socket
    server_sock = socket(AF_INET , SOCK_STREAM , 0);
    if (server_sock == -1)
    {
        printf("Could not create socket to server");
    }
    // puts("Server socket created");

    //Alow the socket to be used immediately
    //This resolves "Address already in use" error
    int option = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
     
    //Prepare the sockaddr_in structure
    struct hostent* hostt;
    hostt = gethostbyname(hostName);
    if(hostt == NULL)
    {
        printf("gethostbyname error: %s\n", strerror(h_errno));
    }

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, hostt->h_addr_list[0], hostt->h_length);
    server.sin_port = htons( 80 );
    socklen_t server_size = sizeof(server);

    int server_conn = connect(server_sock, (struct sockaddr *) &server, server_size);
    if (server_conn == -1) 
    {
        perror("Failed to connect to host");
        close(client_sock);
        return;
    }

    // Forward the HTTP request
    int mesLen = strlen(clientMessage);
    snprintf(clientMessage+mesLen, HUGEBUFSIZE, "\r\n\r\n");
    send(server_sock, clientMessage, strlen(clientMessage), 0);

    int bytesRec = receiveFromServer(server_sock, serverMessage);
    if(bytesRec == 0)
    {
        printf("Host disconnected: %d\n", server_sock);
        fflush(stdout);
    }
    else if(bytesRec == -1)
    {
        fprintf(stderr, "Recv error: \n");
    }

    addPageToCache(serverMessage, requestURI, bytesRec);

    //Send server contents back to client
    send(client_sock, serverMessage, bytesRec, 0);
    close(server_sock);
}

int main(int argc , char *argv[])
{
    int socket_desc , client_sock , read_size;
    struct sockaddr_in server;
    
    // struct config confData;
    int port;
    int errnum = 0;
    pid_t pid;
    cache *returnedPage;

    //Make sure user enters valid number of args
    if(argc < 3)
    {
        printf("USAGE: ./webproxy [PORT NUMBER] [CACHE TIMEOUT]\n");
        return 1;
    }

    port = atoi(argv[1]);
    cacheExpiration = atoi(argv[2]);

    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");

    //Alow the socket to be used immediately
    //This resolves "Address already in use" error
    int option = 1;
    setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");
     
    //Listen
    if ( listen(socket_desc , 10) < 0 )
    {
        perror("Listen failed. Error");
        return 1;
    }
    puts("Waiting for incoming connections...");

    while(1)
    {
        struct sockaddr_in client;
        int c = sizeof(struct sockaddr_in);

        char client_message[LARGEBUFSIZE];
        char requestMethod[MAXBUFSIZE];
        char requestURI[MAXBUFSIZE];
        char requestVersion[MAXBUFSIZE];
        char hostName[MAXBUFSIZE];

        //accept connection from an incoming client
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0)
        {
            perror("accept failed");
            return 1;
        }
        puts("Connection accepted");

        //Fork a child process to handle requests
        if ( (pid = fork()) == 0 )
        {

            //Child closes the listening socket
            close(socket_desc);

            //Do some stuff with client_sock
            //Receive a message from client
            while( (read_size = recv(client_sock , client_message , MAXBUFSIZE , 0)) > 0 )
            {
                memset(&requestMethod, 0, MAXBUFSIZE);
                memset(&requestURI, 0, MAXBUFSIZE);
                memset(&requestVersion, 0, MAXBUFSIZE);

                sscanf(client_message, "%s %s %s", requestMethod, requestURI, requestVersion);

                parseHostName(requestURI, hostName);

                //Pass in 0 to do method, version validation
                errnum = errorHandler(client_sock, 0, requestMethod, requestURI, requestVersion);

                if (errnum == 0) 
                {
                    //Process the client's request
                    // removePageFromCache();
                    returnedPage = findCachePage(requestURI);
                    if(returnedPage == NULL)
                    {
                        printf("No page in cache, requesting from server\n");
                        processRequest(client_sock, client_message, requestURI, hostName);
                    }
                    else
                    {
                        printf("Found the page in the cache, sending from cache\n");
                        strcat(returnedPage->pageData, "\r\n\r\n");
                        send(client_sock, returnedPage->pageData, returnedPage->pageSize, 0);
                    }
                }

                memset(&client_message, 0, MAXBUFSIZE);
            }
             
            if(read_size == 0)
            {
                printf("Client disconnected: %d\n", client_sock);
                fflush(stdout);
            }
            else if(read_size == -1)
            {
                perror("recv failed");
            }

            //Close sockets and terminate child process
            close(client_sock);
            exit(0);
        }

    }
    
    close(socket_desc);
   
    return 0;
}