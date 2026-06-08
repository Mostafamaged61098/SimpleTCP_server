#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
//#include <sys/wait.h>

#define DEFAULT_PORT 8080
#define DEFAULT_IP "127.0.0.1"
#define BUFFER_SIZE 1024

//================= Signal Handling =================//

static volatile sig_atomic_t keep_running = 1;
static int g_sock = -1;

static void handle_sigint(int sig) 
{
    (void)sig;
    keep_running = 0;
    if (g_sock != -1) 
    {
        shutdown(g_sock, SHUT_RDWR);
        close(g_sock);
        g_sock = -1;
    }

}

//================= Create Socket =================//

static int create_socket(void) 
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) 
    {
        perror("Socket creation failed");
        return -1;
    }
    printf("Socket created successfully\n");
    return fd;
}

//================= Parse Target IP and Port =================//

static int parse_target(int argc, char *argv[], char *ip_out, int *port_out) 
{
    char input[64];
    if (argc > 1) 
    {
        strncpy(input, argv[1], sizeof(input) - 1);
        input[sizeof(input) - 1] = '\0';
    } 
    else 
    {
        printf("Enter server IP and port (format IP:PORT) e.g. 192.168.1.1:8080: ");
        if (fgets(input, sizeof(input), stdin) == NULL) 
        {
            perror("Input error");
            return -1;
        }
        input[strcspn(input, "\n")] = '\0';
    }

    char *colon = strchr(input, ':');
    if (!colon) 
    {
        fprintf(stderr, "Invalid format. Use IP:PORT\n");
        return -1;
    }
    *colon = '\0';
    strncpy(ip_out, input, INET_ADDRSTRLEN - 1);
    ip_out[INET_ADDRSTRLEN - 1] = '\0';
    char *port_s = colon + 1;
    int p = atoi(port_s);
    if (p <= 0 || p > 65535) 
    {
        fprintf(stderr, "Invalid port number\n");
        return -1;
    }
    *port_out = p;
    return 0;
}

//================= Configure Server Address =================//

static int configure_server(struct sockaddr_in *srv, const char *ip_str, int port) {
    memset(srv, 0, sizeof(*srv));
    srv->sin_family = AF_INET;
    srv->sin_port = htons(port);
    if (inet_pton(AF_INET, ip_str, &srv->sin_addr) <= 0) 
    {
        perror("Invalid address");
        return -1;
    }
    return 0;
}

//================= Connect to Server =================//

static int connect_server(int client_fd, const struct sockaddr_in *srv) 
{
    if (connect(client_fd, (const struct sockaddr *)srv, sizeof(*srv)) < 0) 
    {
        perror("Connection failed");
        return -1;
    }
    return 0;
}

//================= Send Message =================//

static int send_message(int client_fd, const char *message) 
{
    ssize_t sent = send(client_fd, message, strlen(message), 0);
    if (sent < 0) 
    {
        perror("Send failed");
        return -1;
    }
    return 0;
}

//================= Receive Response =================//

static ssize_t receive_response(int client_fd, char *buffer, size_t size) 
{
    ssize_t bytes = recv(client_fd, buffer, size - 1, 0);
    return bytes;
}

//================= Main Function =================//

int main(int argc, char *argv[]) 
{
    int client_fd = -1;
    int port = DEFAULT_PORT;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};
    char sendbuf[BUFFER_SIZE+20] = {0}; 
    char ip_str[INET_ADDRSTRLEN] = DEFAULT_IP;
    char name[20] = {0};
    

    client_fd = create_socket();
    if (client_fd < 0) 
    {
        return EXIT_FAILURE;
    }

    if (parse_target(argc, argv, ip_str, &port) != 0) 
    {
        printf("[+] Using default target %s:%d\n", ip_str, port);
    }

    if (configure_server(&server_addr, ip_str, port) != 0) 
    {
        close(client_fd);
        return EXIT_FAILURE;
    }

    
    while(1)
    {
        printf("Enter Your Name: ");
        fgets(name, sizeof(name), stdin);

        if (name[0] == '\n') 
        {
            printf("Invalid name, try again.\n");
        }
        else
        {
            break;
        }
    }

    name[strcspn(name, "\n")] = '\0';


    if (connect_server(client_fd, &server_addr) != 0) 
    {
        close(client_fd);
        return EXIT_FAILURE;
    }

    printf("\n[+] Connected to server at %s:%d\n", ip_str, port);

// Main method using select to handle both user input and server messages    
    while(1)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(client_fd, &read_fds);
        int max_fd = client_fd;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) 
        {
            perror("select error");
            break;
        }

        if (FD_ISSET(client_fd, &read_fds)) 
        {
            ssize_t bytes_received = receive_response(client_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) 
            {
                printf("Server disconnected.\n");
                break;
            }
            buffer[bytes_received] = '\0';
            if (strcmp(buffer, "exit") == 0) 
            {
                printf("Server sent exit.\n");
                break;
            }
            printf("%s\n", buffer);
        }
        
        if (FD_ISSET(STDIN_FILENO, &read_fds)) 
        {
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) 
            {
                printf("Error reading input.\n");
                break;
            }

            if (buffer[0] == '\n') 
            {
                continue;
            }

            if (strcmp(buffer, "exit") == 0) 
            {
                printf("%s is exiting.\n", name);
                break;
            }

            buffer[strcspn(buffer, "\n")] = '\0';

            strcat(sendbuf, name);
            strcat(sendbuf, ": ");
            strcat(sendbuf, buffer); 

            if (send_message(client_fd, sendbuf) != 0) 
            {
                break;
            } 

            memset(sendbuf, 0, sizeof(sendbuf));
        }

    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    printf("Client shutting down\n");
    return 0;
}
   
//another method using fork and select
  /*  
    pid_t pid = fork();
    if (pid < 0) 
    {
        perror("fork");
        close(client_fd);
        return EXIT_FAILURE;
    }

    if (pid == 0) 
    {
        
        char sendbuf[BUFFER_SIZE];
        while (1) 
        {
            printf("Client: ");
            if (fgets(sendbuf, sizeof(sendbuf), stdin) == NULL) 
                break;

            size_t len = strlen(sendbuf);
            if (len == 0) 
                continue;

            if (sendbuf[len-1] == '\n') 
                sendbuf[len-1] = '\0';

            if (strcmp(sendbuf, "exit") == 0)
                break;

            if (send_message(client_fd, sendbuf) != 0) 
                break;
        }
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
        _exit(0);
    } 
    else 
    {
    
        while (1) 
        {
            ssize_t bytes_received = receive_response(client_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) 
                break;
                
            buffer[bytes_received] = '\0';
            printf("Server: %s\n", buffer);
            if (strcmp(buffer, "exit") == 0) 
            {
                printf("Server sent exit.\n");
                break;
            }
        }
       
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        /*if (g_sock != -1) 
        {
            close(g_sock);
            g_sock = -1;
        }*/
   