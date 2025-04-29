#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

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

void connectToServer(int sockfd, char* ip, int port)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to %s on port %d\n", ip, port);
}

void sendHTTPRequest(int sockfd, SSL *ssl, char* url, int isHTTPS)
{
    char request[1024];
    sprintf(request, "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", url);

    if (isHTTPS)
        SSL_write(ssl, request, strlen(request));
    else
        send(sockfd, request, strlen(request), 0);

    printf("HTTP request sent!\n");
}

void saveResponseToFile(int sockfd, SSL *ssl, int isHTTPS)
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

    while (1) 
    {
        if (isHTTPS)
            bytes_received = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        else
            bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0)
            break;

        buffer[bytes_received] = '\0';
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    printf("\n\nResponse received. File saved.\n");
}

SSL* initSSL(int sockfd)
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    SSL *ssl = SSL_new((SSL_CTX *)ctx);
    SSL_set_fd(ssl, sockfd);

    if (SSL_connect(ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    printf("SSL/TLS handshake done.\n");

    return ssl;
}

int main()
{
    char url[256];
    printf("Enter a string: ");
    fgets(url, sizeof(url), stdin);
    url[strcspn(url, "\n")] = 0;

    int isHTTPS = 0;    //if this is 0 it is http else it is https
    if (strncmp(url, "https://", 8) == 0)
    {
        isHTTPS = 1;
        memmove(url, url + 8, strlen(url) - 7);
    }
    else if (strncmp(url, "http://", 7) == 0)
    {
        memmove(url, url + 7, strlen(url) - 6);
    }

    char* ip = URLtoIPconverter(url);
    if (strcmp(ip, "error") == 0)
        return 1;

    printf("IP address of %s is: %s\n", url, ip);

    int port = isHTTPS ? 443 : 80;
    int sockfd = createSocket();
    connectToServer(sockfd, ip, port);

    SSL *ssl = NULL;
    if (isHTTPS)
        ssl = initSSL(sockfd);

    sendHTTPRequest(sockfd, ssl, url, isHTTPS);
    saveResponseToFile(sockfd, ssl, isHTTPS);

    if (isHTTPS)
    {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    
    close(sockfd);

    return 0;
}
