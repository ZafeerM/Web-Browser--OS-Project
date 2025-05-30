#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//---------Zafeer's Functions (HTTP and Sockets)----------
char* URLtoIPconverter(char url[256])
{
    struct hostent *host = gethostbyname(url);
    if (host == NULL) 
    {
        fprintf(stderr, "ERROR : Host Name Not Found.\n");
        return "error";
    }

    return inet_ntoa(*((struct in_addr *)host->h_addr_list[0]));
}

int createSocket()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        perror("Socket creation failed");
        exit(1);
    }
    return sockfd;
}

void connectToServer(int sockfd, char* ip)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to %s on port 80\n", ip);
}

void sendHTTPRequest(int sockfd, char* url)
{
    char request[1024];
    sprintf(request, "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", url);
    send(sockfd, request, strlen(request), 0);
    printf("HTTP request sent!\n");
}

void saveResponseToFile(int sockfd)
{
    char buffer[4096];
    int bytes_received;
    FILE *file = fopen("History/output.html", "w");

    if (file == NULL) 
    {
        perror("File opening failed");
        close(sockfd);
        exit(1);
    }

    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) 
    {
        buffer[bytes_received] = '\0';
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    printf("\n\nResponse received. File saved.\n");
}

int main()
{
    char url[256];
    printf("Enter a string: ");
    fgets(url, sizeof(url), stdin);
    url[strcspn(url, "\n")] = 0;

    char* ip = URLtoIPconverter(url);
    if (strcmp(ip, "error") == 0)
        return 1;

    printf("IP address of %s is: %s\n", url, ip);

    int sockfd = createSocket();
    connectToServer(sockfd, ip);
    sendHTTPRequest(sockfd, url);
    saveResponseToFile(sockfd);
    close(sockfd);

    return 0;
}
