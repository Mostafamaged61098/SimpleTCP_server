#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <sys/epoll.h>
#include <unistd.h>
//#include <sys/select.h>
#include <signal.h>
//#include <wait.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 64



//================= Signal Handling =================//

static volatile sig_atomic_t keep_running = 1;
//static int g_sock = -1;

static void handle_sigint(int sig) 
{
    (void)sig;
    keep_running = 0;
    /*if (g_sock != -1) 
    {
        shutdown(g_sock, SHUT_RDWR);
        close(g_sock);
        g_sock = -1;
    }*/
}


//================= Create Socket =================//

static int create_server_socket(void) 
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) 
    {
        perror("[-] Socket creation failed");
        return -1;
    }
    printf("[+] Socket created successfully\n\n");

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        perror("[-] setsockopt failed");
        close(fd);
        return -1;
    }
    return fd;
}

//================= Bind and Listen =================//

static int bind_and_listen(int server_fd, int port) 
{
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("[-] Bind failed");
        return -1;
    }
    printf("[+] Socket bound to port %d\n\n", port);

    if (listen(server_fd, 3) < 0) 
    {
        perror("[-]Listen failed");
        return -1;
    }
    printf("[+] Server listening on port %d...\n\n", port);
    return 0;
}

//================= Accept Client Connection =================//

static int accept_client(int server_fd, struct sockaddr_in *client_addr, socklen_t *addr_len) 
{
    int client_fd = accept(server_fd, (struct sockaddr *)client_addr, addr_len);
    if (client_fd < 0) 
    {
        perror("[-] Accept failed");
        return -1;
    }
    printf("[+] Client connected from %s:%d\n\n",
           inet_ntoa(client_addr->sin_addr),
           ntohs(client_addr->sin_port));
    return client_fd;
}

//================= Receive Message =================//

static ssize_t receive_message(int client_fd, char *buffer, size_t size) 
{
    ssize_t bytes = recv(client_fd, buffer, size - 1, 0);
    return bytes;
}

//================= Send Response =================//

static int send_response(int client_fd, const char *response) 
{
    ssize_t sent = send(client_fd, response, strlen(response), 0);
    if (sent < 0) 
    {
        perror("[-] Send failed");
        return -1;
    }
    return 0;
}

//================= Main Function =================//

int main(int argc, char *argv[]) 
{
    int server_fd = -1;
    int client_fd [MAX_EVENTS] = {-1};
    int connected_clients = 0;
    int port = PORT;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE] = {0};
    char tmp[64];
    char *exit = NULL;

    if (argc >= 2) 
    {
        port = atoi(argv[1]);
    } 
    else 
    {
        printf("Enter port to listen on (1-65535) [%d]: ", PORT);
        if (fgets(tmp, sizeof(tmp), stdin) != NULL) 
        {
            tmp[strcspn(tmp, "\n")] = '\0';
            if (tmp[0] != '\0') 
                port = atoi(tmp);
        }
    }
    if (port <= 0 || port > 65535) 
    {
        fprintf(stderr, "Invalid port number\n");
        return EXIT_FAILURE;
    }

// ----------------------------------------- Server Setup

    server_fd = create_server_socket();
    if (server_fd < 0) 
    {
        return EXIT_FAILURE;
    }

    if (bind_and_listen(server_fd, port) != 0) 
    {
        close(server_fd);
        return EXIT_FAILURE;
    }

// ----------------------------------------- epoll setup
// main method using epoll to handle multiple clients and server input

    int epfd = epoll_create1(0);
    if (epfd == -1) 
    {
        perror("epoll_create1");
        return 1;
    }

    struct epoll_event ev;
    ev.events  = EPOLLIN;       
    ev.data.fd = server_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) 
    {
        perror("epoll_ctl");
        return 1;
    }

    struct epoll_event events[MAX_EVENTS]; 

// ----------------------------------------- Main Loop

    while(1)
    {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (n == -1) 
        {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) 
        {
            int ready_fd = events[i].data.fd;

            if (ready_fd == server_fd) 
            {
                client_fd[connected_clients] = accept_client(server_fd, &client_addr, &client_addr_len);
                if (client_fd[connected_clients] < 0) 
                {
                    continue;
                }
                ev.events  = EPOLLIN;
                ev.data.fd = client_fd[connected_clients];
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd[connected_clients], &ev);
                connected_clients++;
            }
            else
            {
                int bytes = receive_message(ready_fd, buffer, sizeof(buffer));

                exit = strchr(buffer, ':');

                if (bytes <= 0 || strcmp(exit+2, "exit") == 0)
                {  
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ready_fd, NULL);
                    close(ready_fd);
                    for (int j = 0; j < connected_clients; j++) 
                    {
                        if (client_fd[j] == ready_fd) 
                        {
                            client_fd[j] = -1;
                            break;
                        }
                    }
                    connected_clients--;
                    printf("Client %d disconnected.\n", ready_fd);
                } 
                else 
                {
                    buffer[bytes] = '\0';
                    printf("Client %d: %s\n", ready_fd, buffer);
                    for (int j = 0; j < connected_clients; j++) 
                    {
                        if (client_fd[j] != -1 && client_fd[j] != ready_fd) 
                        {
                            send_response(client_fd[j], buffer);
                            strcpy(buffer, "");
                        }
                    }
                }

            }
        }

        if (!keep_running) 
        {
            printf("Shutting down server...\n");
            for (int j = 0; j < connected_clients; j++) 
            {
                if (client_fd[j] != -1) 
                {
                    send_response(client_fd[j], "exit");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd[j], NULL);
                    close(client_fd[j]);
                }
            }
            epoll_ctl(epfd, EPOLL_CTL_DEL, server_fd, NULL);
            close(server_fd);
            break;
        }
    }

    return 0;
}

// ------------------------------------------------------------
// another method using fork and select to handle server input and client messages

    /*while(1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int max_fd = client_fd;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) 
        {
            perror("select error");
            break;
        }

        // Check if data from client
        if (FD_ISSET(client_fd, &readfds)) 
        {
            ssize_t bytes_received = receive_message(client_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) 
            {
                if (bytes_received == 0) 
                {
                    printf("Client disconnected.\n");
                } 
                else 
                {
                    perror("Receive failed");
                }
                break;
            }
            buffer[bytes_received] = '\0';
            printf("Client: %s\n", buffer);
            if (strcmp(buffer, "exit") == 0) 
            {
                printf("Client sent exit.\n");
                break;
            }
        }

        // Check if input from stdin
        if (FD_ISSET(STDIN_FILENO, &readfds)) 
        {
            printf("Server: ");
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) 
            {
                printf("Error reading input.\n");
                break;
            }
            buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline

            if (strcmp(buffer, "exit") == 0) 
            {
                printf("Server exiting.\n");
                break;
            }

            if (send_response(client_fd, buffer) != 0) 
            {
                break;
            }
            printf("Sent: %s\n", buffer);
        }
    }*/

    /*pid_t pid = fork();
    if (pid < 0)    
    {
        perror("fork");
        close(client_fd);
        close(server_fd);
        printf("Server shutting down\n");
        return 0;
    }

    if (pid == 0)
    {
        while(1)
        {
            char sendbuf[BUFFER_SIZE];
            printf("Server: ");
            if (fgets(sendbuf, sizeof(sendbuf), stdin) == NULL)
            {
                printf("Error reading input.\n");
                break; 
            }
            size_t len = strlen(sendbuf);
            if (len == 0)
                continue;

            if (sendbuf[len-1] == '\n')
                sendbuf[len-1] = '\0';

            if (strcmp(sendbuf, "exit") == 0)
            {
                printf("Server exiting.\n");
                break;
            }
            if (send_response(client_fd, sendbuf) != 0)
            {
                break;
            }
        }
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
        _exit(0);
    }
    else
    {
        while(1)
        {
            ssize_t bytes_received = receive_message(client_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) 
                break;

            buffer[bytes_received] = '\0';
            printf("Client: %s\n", buffer);
            if (strcmp(buffer, "exit") == 0) 
            {
                printf("Client sent exit.\n");
                break;
            }
        }
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }*/

    /*close(client_fd);
    close(server_fd);
    printf("Server shutting down\n");*/
   