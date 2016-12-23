// Simple C webserver
// Author: Kevin Holligan
// Network Systems

#include <stdio.h>
#include <string.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
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

#define MAXBUFSIZE 2048

//Create a config structure to store data read from ws.conf
//ws.conf contains service port, document root, default web page, content-type
struct config
{
    int port;
    char root[MAXBUFSIZE];
    char index[MAXBUFSIZE];
    char contentType[MAXBUFSIZE];
};

//Read a configuration file. 
//File has to have same leading token (Listen, DocumentRoot, etc.) as ws.conf 
int readConfigFile(char *fileName, struct config *confData)
{
    FILE *fp;
    char fileBuffer[MAXBUFSIZE];
    char tempBuffer[MAXBUFSIZE];
    const char delimiters[8] = " \n";

    fp = fopen(fileName, "r");
    //If successfully opened file
    if(fp != NULL)
    {
        while(fgets(fileBuffer, MAXBUFSIZE, fp) != NULL)
        {
            //If the line is a commment, do nothing
            if (strncmp(fileBuffer, "#",1) == 0 )
            {
                continue;
            }
            //Preserver our original file buffer (strtok overwrites)
            strncpy(tempBuffer, fileBuffer, MAXBUFSIZE);
            //Read the indicator token
            char *token = strtok(tempBuffer, delimiters);
            
            //Tokenize the second portion of the string and store in config struct
            if(strcmp(token, "Listen") == 0)
            {
                token = strtok(NULL, "\n");
                //Convert port number to an int and store
                sscanf(token, "%d", &confData->port);
            }
            else if(strcmp(token, "DocumentRoot") == 0)
            {
                token = strtok(NULL, "\n");
                strcpy(confData->root, token);
            }
            else if(strcmp(token, "DirectoryIndex") == 0)
            {
                token = strtok(NULL, "\n");
                strcpy(confData->index, token);
            }
            else
            {
                //Append multiple lines to contentType
                strcat(confData->contentType, fileBuffer);
            }
        }
    }
    //Print error and return -1
    else
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 1;
}

//This function provided by ThiefMaster on stackoverflow
// http://stackoverflow.com/questions/5309471/getting-file-extension-in-c
// Adapted and modified for personal use
void getFileExtension( char *buffer, const char* fileName, struct config *confData)
{
    char contentType[MAXBUFSIZE];

    //Find the last occurence in the string
    const char *dot = strrchr(fileName, '.');
    if(!dot || dot == fileName)
    {
        snprintf(buffer, MAXBUFSIZE, "%s", "");
    }
    strcpy(contentType, confData->contentType);
    char* token = strtok(contentType, " \n");
    //Match the first token, then scan through the rest
    if(strcmp(dot, ".html") == 0)
    {
        token = strtok(NULL, " \n");
    }
    else
    {
        //Scan remaining list of tokens
        while( !(strcmp(dot, token) == 0) )
        {
            token = strtok(NULL, " \n");
        }

        //One more time to retrieve the content-type
        token = strtok(NULL, " \n");
    }
    //Append content type to buffer and send it back
    snprintf(buffer, MAXBUFSIZE, "%s", token);
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
        //501 Error: request methods that have not been implemented
        if( !(strcmp(requestMethod, "GET") == 0) )
        {
            //Print the string to errorContent
            snprintf(errorContent, MAXBUFSIZE, "<html><body>501 Not Implemented "
                "Request Method: %s</body></html>", requestMethod);

            //Length keeps track of position in string.
            //Create the header structure
            length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 501 Not Implemented\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
            length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));
            //Send the header to the client
            send(client_sock, errorHeader, strlen(errorHeader), 0);
            //Write data to the client
            write(client_sock, errorContent, strlen(errorContent));

            return -1;
        }
        if ( !( (strcmp(requestVersion, "HTTP/1.1") == 0) || (strcmp(requestVersion, "HTTP/1.0") == 0) ) )
        {
            snprintf(errorContent, MAXBUFSIZE, "<html><body>400 Bad Request Reason: "
                "Invalid Version:%s</body></html>", requestVersion);

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
    else
    {
        //If we want to output a specific status code
        switch(statusCode)
        {
            //File not found
            case 404:
                snprintf(errorContent, MAXBUFSIZE, "<html><body>404 Not Found "
                "Reason URL does not exist: %s</body></html>", requestURI);

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
                    "Cannot allocate memory</body></html>");
                send(client_sock, errorHeader, strlen(errorHeader), 0);
                
                write(client_sock, errorContent, strlen(errorContent));
                return -1;
                break;
            //Duplicate, but in case we want to call it specifically
            case 501:
                snprintf(errorContent, MAXBUFSIZE, "<html><body>501 Not Implemented "
                "Method: %s</body></html>", requestMethod);

                length += snprintf(errorHeader, MAXBUFSIZE, "HTTP/1.1 501 Not Implemented\r\n");
                length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Type: text/html\r\n");
                length += snprintf(errorHeader+length, MAXBUFSIZE-length, "Content-Length: %lu\r\n\r\n", strlen(errorContent));
                send(client_sock, errorHeader, strlen(errorHeader), 0);
                
                write(client_sock, errorContent, strlen(errorContent));
                return -1;
                break;
        }
    }

    return 0;
}

//Handles client requests
void processRequest(int client_sock, char* requestMethod, char* requestURI, char* requestVersion, struct config *confData)
{
    char path[MAXBUFSIZE];
    char contentHeader[MAXBUFSIZE];

    int length = 0;
    int foundFile = 0;

    FILE *fp;

    memset(&path, 0, MAXBUFSIZE); 

    //Check for a default path that reroutes to index.html
    if( (strcmp(requestURI, "/") == 0) || strcmp(requestURI, "/index.html") == 0 ||
        strcmp(requestURI, "/index.htm") == 0 || strcmp(requestURI, "/index.ws") == 0)
    {
        //Add the root to the path and force add index.html   
        snprintf(path, MAXBUFSIZE, "%s%s", confData->root, "/index.html");
        //Change the requested URI to index.html so we can process the apage
        snprintf(requestURI, MAXBUFSIZE, "%s", "/index.html");
        fp = fopen(path, "r");
    }
    else
    {
        //Create the full path to find files
        snprintf(path, MAXBUFSIZE, "%s%s", confData->root, requestURI);
        fp = fopen(path, "r");
    }

    //Make sure the file is found, otherwise call 404
    if(fp != NULL)
    {
        foundFile = 1;
    }

    if(foundFile)
    {
        char *fileExtension = malloc(MAXBUFSIZE);
        //Get the file extension, content-type
        getFileExtension(fileExtension, requestURI, confData);

        //Get the length of the content
        fseek(fp, 0, SEEK_END);
        long fileSize = ftell(fp);
        rewind(fp);

        //Create the content header
        length += snprintf(contentHeader, MAXBUFSIZE, "HTTP/1.1 200 OK\r\n");
        length += snprintf(contentHeader+length, MAXBUFSIZE-length, "Content-Type: %s\r\n", fileExtension);
        length += snprintf(contentHeader+length, MAXBUFSIZE-length, "Content-Length: %ld\r\n", fileSize);
        length += snprintf(contentHeader+length, MAXBUFSIZE-length, "Connection: keep-alive\r\n\r\n");

        //send the header to the client
        send(client_sock, contentHeader, strlen(contentHeader), 0);

        //Read in the content data
        char* fileBuffer;
        fileBuffer = malloc(MAXBUFSIZE);
        size_t bytesRead = 0;

        //Test that we are still reading content and have not reached eof
        while( (bytesRead = fread(fileBuffer, 1, MAXBUFSIZE, fp) )> 0 || !feof(fp) ) 
        {
            //send content data to client
            write(client_sock, fileBuffer, (int)bytesRead);
        }

        //free resources
        free(fileBuffer);
        free(fileExtension);
        fclose(fp);
    }
    else
    {
        //404 error if file not found
        errorHandler(client_sock, 404, requestMethod, requestURI, requestVersion);
    }
}

int main(int argc , char *argv[])
{
    int socket_desc , client_sock , c , read_size;
    struct sockaddr_in server , client;
    
    struct config confData;
    int status;
    int errnum = 0;
    char *fileName;
    pid_t pid;
    struct timeval keepAlive;

    // Set the keep alive value 
    keepAlive.tv_sec = 10;

    //Config file
    fileName = "./ws.conf";

    //Read config file into config struct
    status = readConfigFile(fileName, &confData);
    if(status == -1)
    {
        fprintf(stderr, "Error opening file %s: %s\n", fileName, strerror(errnum));
        return 1;
    }

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
    server.sin_port = htons( confData.port );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");
     
    //Listen
    if ( listen(socket_desc , 3) < 0 )
    {
        perror("Listen failed. Error");
        return 1;
    }
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);

    while(1)
    {
        char client_message[MAXBUFSIZE];
        char requestMethod[MAXBUFSIZE];
        char requestURI[MAXBUFSIZE];
        char requestVersion[MAXBUFSIZE];
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
                //clear the buffers
                memset(&requestMethod, 0, MAXBUFSIZE);
                memset(&requestURI, 0, MAXBUFSIZE);
                memset(&requestVersion, 0, MAXBUFSIZE);

                printf("Client socket num: %d\n", client_sock);
                printf("Client_message: %s", client_message);

                //read in the client request in format "[METHOD] [URI] [VERSION]"
                sscanf(client_message, "%s %s %s", requestMethod, requestURI, requestVersion);

                //Pass in 0 to do method, version validation
                errnum = errorHandler(client_sock, 0, requestMethod, requestURI, requestVersion);

                if (errnum == 0) 
                {
                    //Process the client's request
                    processRequest(client_sock, requestMethod, requestURI, requestVersion, &confData);
                }
                else
                {
                    //Something went wrong, close the client connection
                    printf("Client disconnected: %d\n", client_sock);
                    close(client_sock);
                    exit(0);
                }
                // processRequest(client_sock, requestMethod, requestURI, &confData);
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
    
    // Close the parent socket
    printf("Server closed: %d\n", socket_desc);
    close(socket_desc);
   
    return 0;
}
