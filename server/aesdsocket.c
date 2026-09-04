#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h> // Added for inet_ntop

#include <sys/queue.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024
#define TIME_SIZE 64

int server_fd = -1;
struct addrinfo *server_info;
FILE *file;
pthread_mutex_t file_lock, list_lock;

struct Client
{
    pthread_t thread;
    bool completed;
    int client_fd;
    SLIST_ENTRY(Client)
    clients;
};
SLIST_HEAD(client_head, Client);
struct client_head c_head;

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        // Finish all threads which are pending then exit
        void *result;
        struct Client *np;
        pthread_mutex_lock(&list_lock);
        SLIST_FOREACH(np, &c_head, clients)
        {
            if (pthread_join(np->thread, &result))
            {
                perror("while joining completed thread");
            }
        }
        pthread_mutex_unlock(&list_lock);

        // Close file
        fclose(file);

        syslog(LOG_DEBUG, "Caught signal, exiting");

        if (!access(DATA_FILE, F_OK))
            if (unlink(DATA_FILE) == -1)
            {
                perror("Could not delete the file");
            }

        if (server_info != NULL)
        {
            freeaddrinfo(server_info);
        }

        if (server_fd != -1)
        {
            syslog(LOG_DEBUG, "Closing socket");
            close(server_fd);
        }

        syslog(LOG_DEBUG, "Closing syslog");
        closelog();

        exit(0);
    }
    else if (signal == SIGALRM)
    {
        alarm(10);
        char buffer[TIME_SIZE];
        time_t raw_time = time(NULL);
        if (raw_time == (time_t)-1)
        {
            perror("could not get current time");
            return;
        }

        struct tm *time_info = localtime(&raw_time);
        if (time_info == NULL)
        {
            perror("Failed to convert time");
            return;
        }
        const char *format = "timestamp: %Y-%m-%d %H:%M:%S\n";
        size_t written = strftime(buffer, sizeof(buffer), format, time_info);
        pthread_mutex_lock(&file_lock);
        fwrite(buffer, sizeof(char), written, file);
        pthread_mutex_unlock(&file_lock);
    }
}

void *client_handler(void *client)
{
    char recv_buffer[BUFFER_SIZE];

    int data_to_recv;

    pthread_mutex_lock(&file_lock);

    while ((data_to_recv = recv(((struct Client *)client)->client_fd, recv_buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        if (fwrite(recv_buffer, sizeof(char), data_to_recv, file) == 0)
        {
            perror("Could not write to file");
            return NULL;
        }

        fflush(file);

        // Check if newline exists in the currently received chunk
        char *newline = memchr(recv_buffer, '\n', data_to_recv);
        if (newline != NULL)
        {
            fseek(file, 0, SEEK_SET);
            char send_buffer[BUFFER_SIZE];
            int data_to_send;
            while ((data_to_send = fread(send_buffer, 1, BUFFER_SIZE - 1, file)) > 0)
            {
                if (send(((struct Client *)client)->client_fd, send_buffer, data_to_send, 0) < 0)
                {
                    perror("Could not send data to client");
                    return NULL;
                }
            }
            fseek(file, 0, SEEK_END);
        }
    }

    pthread_mutex_unlock(&file_lock);

    if (data_to_recv < 0)
    {
        perror("Error while receiving data from client");
    }

    close(((struct Client *)client)->client_fd);
    ((struct Client *)client)->completed = true;
    return NULL;
}

void create_daemon()
{
    pid_t pid;

    // 1. The Only Fork
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE); // Fork failed
    if (pid > 0)
        exit(EXIT_SUCCESS); // Parent exits, child continues in background

    // 2. Create a new session (disconnects from the terminal)
    if (setsid() < 0)
        exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
        create_daemon();
    struct sigaction sa;

    // setup signal action handlers
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGALRM, &sa, 0) == -1)
    {
        perror("Error while setting signal handlers");
        raise(SIGINT);
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    const char *port = "9000";
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, port, &hints, &server_info);
    if (status != 0)
    {
        perror("getaddrinfo error");
        raise(SIGINT);
    }

    server_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (server_fd == -1)
    {
        perror("Error while creating socket");
        freeaddrinfo(server_info);
        raise(SIGINT);
    }

    // Allow socket address reuse immediately after shutdown
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(server_fd, server_info->ai_addr, server_info->ai_addrlen) < 0)
    {
        perror("Binding failed");
        freeaddrinfo(server_info);
        raise(SIGINT);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listening to port failed");
        raise(SIGINT);
    }

    // Open file for writing
    file = fopen(DATA_FILE, "w+");
    if (file == NULL)
    {
        perror("Could not create or open file /var/tmp/aesdsocketdata");
        raise(SIGINT);
    }

    if (pthread_mutex_init(&file_lock, NULL) != 0)
    {
        perror("File lock mutex init failed");
        raise(SIGINT);
    }

    if (pthread_mutex_init(&list_lock, NULL) != 0)
    {
        perror("File lock mutex init failed");
        raise(SIGINT);
    }

    // Create linked list definition
    SLIST_INIT(&c_head);

    // Set alarm every 10 seconds
    alarm(10);

    while (1)
    {
        struct sockaddr_in client_addr; // Fixed structure type
        socklen_t addr_len = sizeof(client_addr);

        int client_fd;

        do
        {
            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        } while (client_fd == -1 && errno == EINTR);

        if (client_fd == -1)
        {
            perror("ERROR: Accept failed");
            raise(SIGINT);
        }

        // Beautifully parse the IP to a string for syslog assignment specs
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

        syslog(LOG_DEBUG, "Accepted connection from %s", ip_str);

        // Create a thread that handles client and add to linked list
        struct Client *thread_node = malloc(sizeof(struct Client));
        if (thread_node == NULL)
        {
            perror("malloc failed for thread_node");
            close(client_fd);
            continue;
        }
        thread_node->completed = false;
        thread_node->client_fd = client_fd;

        if (pthread_create(&(thread_node->thread), NULL, client_handler, thread_node))
        {
            perror("Could not create client thread");
            free(thread_node);
            continue;
        }

        // Inserts new_node at the very front of the list
        pthread_mutex_lock(&list_lock);
        SLIST_INSERT_HEAD(&c_head, thread_node, clients);
        pthread_mutex_unlock(&list_lock);

        // Join threads which are completed
        void *result;
        struct Client *np;

        pthread_mutex_lock(&list_lock);
        SLIST_FOREACH(np, &c_head, clients)
        {
            if (np->completed)
            {
                if (pthread_join(np->thread, &result))
                {
                    perror("while joining completed thread");
                }
                SLIST_REMOVE(&c_head, np, Client, clients);
            }
        }
        pthread_mutex_unlock(&list_lock);

        syslog(LOG_DEBUG, "Closed connection from %s", ip_str);
    }

    return 0;
}