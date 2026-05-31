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

#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

int server_fd = -1;
struct addrinfo *server_info;

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
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
}

void client_handler(int client_fd)
{
    char recv_buffer[BUFFER_SIZE];

    FILE *file = fopen(DATA_FILE, "a+");
    if (file == NULL)
    {
        perror("Could not create or open file /var/tmp/aesdsocketdata");
        close(client_fd);
        return;
    }

    int data_to_recv;
    while ((data_to_recv = recv(client_fd, recv_buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        if (fwrite(recv_buffer, sizeof(char), data_to_recv, file) == 0)
        {
            perror("Could not write to file");
            fclose(file);
            return;
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
                if (send(client_fd, send_buffer, data_to_send, 0) < 0)
                {
                    perror("Could not send data to client");
                    fclose(file);
                    return;
                }
            }
            fseek(file, 0, SEEK_END);
        }
    }

    if (data_to_recv < 0)
    {
        perror("Error while receiving data from client");
    }

    fclose(file);
    close(client_fd);
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
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("Error while setting signal handlers");
        exit(1);
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
        exit(1);
    }

    server_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (server_fd == -1)
    {
        perror("Error while creating socket");
        freeaddrinfo(server_info);
        exit(1);
    }

    // Allow socket address reuse immediately after shutdown
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(server_fd, server_info->ai_addr, server_info->ai_addrlen) < 0)
    {
        perror("Binding failed");
        freeaddrinfo(server_info);
        exit(1);
    }

    freeaddrinfo(server_info); // Safe to free after binding

    if (listen(server_fd, 10) < 0)
    {
        perror("Listening to port failed");
        exit(1);
    }

    while (1)
    {
        struct sockaddr_in client_addr; // Fixed structure type
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1)
        {
            perror("ERROR: Accept failed");
            continue;
        }

        // Beautifully parse the IP to a string for syslog assignment specs
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

        syslog(LOG_DEBUG, "Accepted connection from %s", ip_str);

        client_handler(client_fd);

        syslog(LOG_DEBUG, "Closed connection from %s", ip_str);
    }

    return 0;
}