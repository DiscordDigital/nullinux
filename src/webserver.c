#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <getopt.h>
#include <ctype.h>
#include <openssl/evp.h>
#include <signal.h>
#include <sys/wait.h>

#define BUFFER 4096 // Size of the I/O buffer used throughout the program

/* --------------------------------------------------------------
 * Signal handler: reap terminated child processes to avoid zombies.
 * -------------------------------------------------------------- */
void reap_children(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* --------------------------------------------------------------
 * Helper: send a minimal 404 Not-Found response.
 * -------------------------------------------------------------- */
void send_404(int client_fd) {
    const char *msg = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
    (void)!write(client_fd, msg, strlen(msg));
}

/* --------------------------------------------------------------
 * Helper: send a minimal 401 Unauthorized response with a Basic
 * authentication challenge.
 * -------------------------------------------------------------- */
void send_401(int client_fd) {
    const char *msg = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Restricted\"\r\nContent-Length: 12\r\n\r\nUnauthorized";
    (void)!write(client_fd, msg, strlen(msg));
}

/* --------------------------------------------------------------
 * Send the contents of a regular file.
 *   client_fd - socket to the HTTP client
 *   path      - filesystem path to the file
 *   size      - file size (already known via stat())
 * -------------------------------------------------------------- */
void send_file(int client_fd, const char *path, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { send_404(client_fd); return; }

    dprintf(client_fd, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", size);

    char buf[BUFFER];
    ssize_t n;
    while ((n = read(fd, buf, BUFFER)) > 0) {
        (void)!write(client_fd, buf, n);
    }
    close(fd);
}

/* --------------------------------------------------------------
 * Generate and send an HTML directory listing.
 *   client_fd - socket to the HTTP client
 *   path      - filesystem directory to read (note: underscore is part of the original name)
 *   url_path  - URL path used to build the links (again, original name)
 * -------------------------------------------------------------- */
void send_dir(int client_fd, const char *path, const char *url_path) {
    DIR *dir = opendir(path); // open the directory
    if (!dir) { send_404(client_fd); return; }

    // Build a simple HTML page in memory
    char html[65536];
    size_t pos = snprintf(html, sizeof(html),
        "<html><head><link rel=\"stylesheet\" href=\"/styles.css\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head><body><ul>");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) continue; // skip the current-dir entry

        // Construct a relative URL for each entry
        char link[512];
        snprintf(link, sizeof(link), "%s%s%s",
            url_path, url_path[strlen(url_path)-1]=='/'?"":"/", entry->d_name);

        // Append a list item - guard against overflow
        int n = snprintf(html + pos, sizeof(html) - pos,
            "<li><a href=\"%s\">%s</a></li>", link, entry->d_name);

        if (n < 0 || (size_t)n >= sizeof(html) - pos) break; // stop if buffer full
        pos += n;
    }
    closedir(dir);
    
    // Close the HTML list and document
    pos += snprintf(html + pos, sizeof(html) - pos, "</ul></body></html>");

    // Send HTTP header + HTML payload
    dprintf(client_fd,
        "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\nContent-Type: text/html\r\n\r\n",
        pos);
    (void)!write(client_fd, html, pos);
}

/* --------------------------------------------------------------
 * OpenSSL helper: build a Base64-encoded "user:pass" string for
 * HTTP Basic authentication.
 * -------------------------------------------------------------- */
char *basic_auth_string(const char *user, const char *pass) {
    char creds[512];
    snprintf(creds, sizeof(creds), "%s:%s", user, pass);

    size_t outlen = 4*((strlen(creds)+2)/3)+1; // calculate needed output length
    char *out = malloc(outlen);
    if (!out) return NULL;

    EVP_EncodeBlock((unsigned char*)out, (unsigned char*)creds, strlen(creds));
    return out;
}

/* --------------------------------------------------------------
 * Verify the Authorization header against the expected Base64
 * credential string.
 *   header    - raw HTTP request buffer (contains headers)
 *   expected  - Base64-encoded "user:pass" string generated above
 * -------------------------------------------------------------- */
int check_auth(const char *header, const char *expected) {
    if (!header) return 0;

    const char *prefix = "Authorization: Basic ";
    const char *p = strstr(header, prefix);
    if (!p) return 0;

    p += strlen(prefix);

    // Extract the credential token up to CR or LF
    char received[512] = {0};
    int i = 0;
    while (*p && *p != '\r' && *p != '\n' && i < sizeof(received)-1) {
        received[i++] = *p++;
    }
    received[i] = '\0';

    return strcmp(received, expected) == 0;
}

/* --------------------------------------------------------------
 * Decode a percent-encoded URL component into plain text.
 *   dst  - destination buffer
 *   src  - source (encoded) string
 * -------------------------------------------------------------- */
void url_decode(char *dst, const char *src) {
    while (*src) {
        if (*src == '%') {
            if (isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
                char hex[3] = { src[1], src[2], 0 };
                *dst++ = (char) strtol(hex, NULL, 16);
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* --------------------------------------------------------------
 * Per-client request handler. Runs in a forked child process.
 *   client_fd   - socket descriptor for the connected client
 *   client_addr - sockaddr_in structure describing the peer
 *   auth_b64    - optional Base64 credential string (NULL if auth disabled)
 * -------------------------------------------------------------- */
void handle_client(int client_fd, struct sockaddr_in *client_addr, const char *auth_b64) {
    char buffer[BUFFER];
    ssize_t r = read(client_fd, buffer, BUFFER - 1);
    if (r <= 0) { close(client_fd); exit(0); }
    buffer[r] = 0; // NUL-terminate the request

    // Parse the request line (method and URL)
    char method[8], url[256];
    sscanf(buffer, "%s %s", method, url);

    // Resolve client IP address for logging
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, ip, sizeof(ip));
    
    // Log all requests except static assets that are served automatically
    if (strcmp(url, "/styles.css") != 0 && strcmp(url, "/favicon.ico") != 0)
        fprintf(stderr, "%s -> %s\n", ip, url);

    // Enforce Basic authentication if a credential string was supplied
    if (auth_b64 && !check_auth(buffer, auth_b64)) {
        send_401(client_fd);
        close(client_fd);
        exit(0);
    }

    // Only GET is implemented; other methods are ignored
    if (strcmp(method, "GET") == 0) {
        char path[512];
        char decoded_url[512];
        char testIndex[523];
        
        size_t max_len = sizeof(path) - 2; // leave room for leading '.' and NUL
        
        // Decode any percent-escapes in the URL
        url_decode(decoded_url, url);
        if (strlen(decoded_url) > max_len) {
            decoded_url[max_len] = '\0';  // truncate if overly long
        }
        
        // Map the URL to a filesystem path
        if (strcmp(url, "/styles.css") == 0)
            snprintf(path, sizeof(path), "/opt/webserver/styles.css");
        else
            snprintf(path, sizeof(path), ".%.*s", (int)(sizeof(path)-2), decoded_url);

        // Determine the type of the target (file, directory, etc.)
        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode)) send_file(client_fd, path, st.st_size);
            else if (S_ISDIR(st.st_mode)) {
                // Test if an index.html file exists
                snprintf(testIndex, sizeof(testIndex), "%s/index.html", path);
                if (stat(testIndex, &st) == 0) {
                    send_file(client_fd, testIndex, st.st_size);
                } else {
                    send_dir(client_fd, path, decoded_url);
                }
            } else send_404(client_fd);
        } else send_404(client_fd);
    }

    // Clean up the client connection
    close(client_fd);
    exit(0);
}

/* --------------------------------------------------------------
 * Program entry point - parses command-line options, sets up the
 * listening socket, and forks a new process for each incoming
 * connection.
 * -------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    int port = 80; // default HTTP port
    char *username = NULL, *password = NULL; // command-line credentials (if any)

    // Ignore SIGPIPE so that write() errors on closed sockets don't kill the server
    signal(SIGPIPE, SIG_IGN);

    // Process command-line flags: -p <port> -u <user> -P <pass>
    int opt;
    while ((opt = getopt(argc, argv, "p:u:P:")) != -1) {
        switch (opt) {
            case 'p': port = atoi(optarg); break;
            case 'u': username = optarg; break;
            case 'P': password = optarg; break;
            default:
                fprintf(stderr, "Usage: %s [-p port] [-u user -P pass]\n", argv[0]);
                exit(1);
        }
    }

    // If a username was supplied, a password must also be present
    if (username && !password) {
        fprintf(stderr, "Error: Password must be provided with username.\n");
        exit(1);
    }

    // Prepare the Base64 credential string for later checks
    char *auth_b64 = NULL;
    if (username && password) auth_b64 = basic_auth_string(username, password);

    // Create a TCP socket for listening
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    // Enable address/port reuse (helps during rapid restarts)
    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // Bind the socket to the chosen port on all interfaces
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    
    // Mark the socket as a passive listening socket
    if (listen(server_fd, 10) < 0) { perror("listen"); exit(1); }
    
    // Install a SIGCHLD handler to reap child processes
    signal(SIGCHLD, reap_children);

    fprintf(stderr, "Serving current directory on port %d\n", port);

    // Main accept loop - one forked child per client
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process: handle the request, then exit
            close(server_fd); // child does not need the listening socket
            handle_client(client_fd, &client_addr, auth_b64);
        } else if (pid > 0) {
            // Parent process: close its copy of the client socket and continue
            close(client_fd); // parent closes client FD
        } else {
            // Fork failed - report and continue
            perror("fork");
            close(client_fd);
        }
    }

    // Clean-up (unreachable in current design, but kept for completeness)
    free(auth_b64);
    close(server_fd);
    return 0;
}
