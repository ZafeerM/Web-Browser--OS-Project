// gcc `pkg-config --cflags --libs gtk+-3.0` main.c -o main
// compile with this.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <semaphore.h>

#define SEM_NAME "/web_tab_ready"

pid_t tab_id[100]; // Global Variable so That main can detect when thread create a child for tab and make GUI
int counter = 0;
GtkWidget *window;
GtkWidget *Notebook, *Content_In_Page[100], *Page_Label[100];
int pipefd[2]; // to check host name validity and communicate to parent process 

gboolean graceful_shutdown()
{
    // this function should. close window, kill all children(lmao), and finally exit main function somehow

    gtk_main_quit(); // this will close window
    return FALSE;
}

char *URLtoIPconverter(char url[256])
{
    struct hostent *host = gethostbyname(url);
    if (host == NULL)
    {
        fprintf(stderr, "ERROR  Host Name Not Found.\n");
        write(pipefd[1],"1",1);
        return "error";
    }
    else
    {
        write(pipefd[1],"0",1);    
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

void connectToServer(int sockfd, char *ip)
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

void sendHTTPRequest(int sockfd, char *url)
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

    // set up IPC
    char *name = "/Web_Data";
    const int SIZE = 4096 * 32;
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("shm_open failed when trying to write");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, SIZE) == -1)
    {
        perror("ftruncate failed");
        close(fd);
        shm_unlink(name); // Clean up if it failed
        exit(EXIT_FAILURE);
    }

    char *ptr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap failed");
        close(fd);
        shm_unlink(name);
        exit(EXIT_FAILURE);
    }

    memset(ptr, 0, SIZE);

    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytes_received] = '\0';
        fwrite(buffer, 1, bytes_received, file);

        // this section will write the data to a shared memory
        sprintf(ptr, "%s", buffer);
        ptr += strlen(buffer);
    }

    fclose(file);
    close(fd);
    printf("\n\n\nResponse received. File saved.\n");

    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);
    if (sem == SEM_FAILED)
    {
        perror("sem_open failed in child");
        exit(1);
    }
    sem_post(sem);
    sem_close(sem);
}

int handle_URL(char *url)
{
    url[strcspn(url, "\n")] = 0;

    char *ip = URLtoIPconverter(url);
    if (strcmp(ip, "error") == 0)
        return 1;

    printf("IP address of %s is %s\n", url, ip);

    int sockfd = createSocket();

    connectToServer(sockfd, ip);
    sendHTTPRequest(sockfd, url);
    saveResponseToFile(sockfd);
    close(sockfd);
}

void *Read_URL(void *arg) // Worker Thread To constantly await URL in main terminal
{
    char url[256];

    while (1)
    {
        printf("Enter a string : ");
        fgets(url, sizeof(url), stdin);
        url[strcspn(url, "\n")] = '\0';
        if (strcmp(url, "exit") == 0)
        {
            break;
        }
        else if (strcmp(url, "history") == 0) // USER CAN VIEW HISTORY FROM HERE
        {
        }
        else
        {
            pipe(pipefd);
            char *flag;
            tab_id[counter] = fork();
            if (tab_id[counter] < 0)
            {
                printf("Error Creating Process for Tab!\n");
                exit(1);
            }
            else if (tab_id[counter] == 0)
            {
                if(handle_URL(url) == 1)
                {
                    exit(0);
                }
                else
                {
                    exit(0); // To be Changed
                }
            }
            counter++;
            sleep(1);
            read(pipefd[0],flag,1);
            if(atoi(flag) == 1)
            {
                counter--;
            }
        }
    }
    pthread_exit(NULL);
}

void Tab_Reload(gpointer arg)
{
    
}

gboolean check_shared_memory(gpointer user_data) // checks for new tab via counter Periodically
{
    // handle synchronization to handle parallelism
    // static int last_counter = 0;

    char *ptr;
    int SIZE = 4096 * 32;
    char *name = "/Web_Data";

    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 0); // Because this function is called presistently sem has to open repeatedly and closed. causing overhead. Fix if time
    if (sem == SEM_FAILED)
    {
        perror("sem_open failed in main");
        exit(1);
    }
    // if(last_counter < counter)
    if (sem_trywait(sem) == 0)
    {
        int fd = shm_open(name, O_RDONLY, 0666);
        if (fd == -1)
        {
            perror("shm_open failed in main");
            exit(EXIT_FAILURE);
        }

        ptr = (char *)mmap(0, SIZE, PROT_READ, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED)
        {
            perror("mmap failed");
            close(fd);
            shm_unlink(name);
            exit(EXIT_FAILURE);
        }

        char str[16];
        sprintf(str, "Tab %d", counter);
    
        Page_Label[counter-1] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        GtkWidget* label = gtk_label_new(str);

        GtkWidget* closeButton = gtk_button_new();
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale("/home/kumail/Desktop/OS Project/closeIcone.png", 16, 16, TRUE, NULL);
        GtkWidget* closeIcon = gtk_image_new_from_pixbuf(pixbuf); // insert image path here
        g_object_unref(pixbuf);
        gtk_button_set_relief(GTK_BUTTON(closeButton), GTK_RELIEF_NONE);
        gtk_button_set_image(GTK_BUTTON(closeButton), closeIcon);
        gtk_widget_set_focus_on_click(closeButton, FALSE);

        GtkWidget* ReloadButton = gtk_button_new();
        pixbuf = gdk_pixbuf_new_from_file_at_scale("/home/kumail/Desktop/OS Project/reloadIcon.png", 16, 16, TRUE, NULL);
        GtkWidget* ReloadIcon = gtk_image_new_from_pixbuf(pixbuf); // insert image path here
        g_object_unref(pixbuf);
        gtk_button_set_relief(GTK_BUTTON(ReloadButton), GTK_RELIEF_NONE);
        gtk_button_set_image(GTK_BUTTON(ReloadButton), ReloadIcon);
        gtk_widget_set_focus_on_click(ReloadButton, FALSE);

        gtk_box_pack_start(GTK_BOX(Page_Label[counter-1]), label, FALSE, FALSE,0); // test expand and fill parametes
        gtk_box_pack_start(GTK_BOX(Page_Label[counter-1]), ReloadButton, FALSE, FALSE,1);
        gtk_box_pack_start(GTK_BOX(Page_Label[counter-1]), closeButton, FALSE, FALSE,1);

        gtk_widget_show_all(Page_Label[counter-1]);

        g_signal_connect_swapped(ReloadButton, "clicked", G_CALLBACK(Tab_Reload), NULL);


        GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

        Content_In_Page[counter-1] = gtk_text_view_new();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(Content_In_Page[counter-1]), GTK_WRAP_WORD);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(Content_In_Page[counter-1]), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(Content_In_Page[counter-1]), FALSE);

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(Content_In_Page[counter-1]));
        gtk_text_buffer_set_text(buffer, ptr, -1);

        gtk_container_add(GTK_CONTAINER(scrolled_window), Content_In_Page[counter-1]);

        g_signal_connect_swapped(closeButton, "clicked", G_CALLBACK(gtk_widget_destroy), scrolled_window);


        gtk_notebook_append_page(GTK_NOTEBOOK(Notebook), scrolled_window, Page_Label[counter-1]);

        // last_counter = counter;
    }
    gtk_widget_show_all(window);
    sem_close(sem);
    return TRUE;
}

int main(int argc, char *argv[])
{
    pthread_t Worker_threadID;
    int r = pthread_create(&Worker_threadID, NULL, Read_URL, NULL);

    // Set Up GTK GUI
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);                              // create main window
    g_signal_connect(window, "delete_event", G_CALLBACK(graceful_shutdown), NULL); // handles exit(cross) button of window
    gtk_window_set_default_size(GTK_WINDOW(window), 750,200);
    Notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(Notebook), GTK_POS_TOP);
    gtk_container_add(GTK_CONTAINER(window), Notebook);

    g_timeout_add(500, check_shared_memory, NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}