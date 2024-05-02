#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // Include for inet_ntoa
#include <sstream>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h> // For directory handling

// Function to split string by delimiter
std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool is_directory(const std::string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) return false;
    return S_ISDIR(statbuf.st_mode);
}

void serve_directory_listing(int connfd, const std::string& path) {
    DIR *dir;
    struct dirent *ent;
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    response += "<html><body><h1>Directory Listing of " + path + "</h1><ul>";

    if ((dir = opendir(path.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            response += "<li><a href='" + std::string(ent->d_name) + "'>" + std::string(ent->d_name) + "</a></li>";
        }
        closedir(dir);
        response += "</ul></body></html>";
    } else {
        response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nDirectory not found";
    }

    write(connfd, response.c_str(), response.length());
}

// Serve the file content
void serve_file(int connfd, const std::string& path) {
    if (is_directory(path)) {
        serve_directory_listing(connfd, path);
        return;
    }
    
    std::ifstream file(path, std::ifstream::binary);
    if (file) {
        // Get length of file:
        file.seekg(0, file.end);
        int length = file.tellg();
        file.seekg(0, file.beg);

        char* buffer = new char[length];

        // Read data as a block:
        file.read(buffer, length);

        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(length) + "\r\n\r\n";
        write(connfd, response.c_str(), response.length());
        write(connfd, buffer, length);

        delete[] buffer;
    } else {
        std::string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found";
        write(connfd, response.c_str(), response.length());
    }
    file.close();
}

// Execute a CGI script and send the output
void execute_cgi_script(int connfd, const std::string& path) {
    // Assuming the script is executable and prints the correct HTTP headers
    std::string output = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body>Executed CGI</body></html>";
    write(connfd, output.c_str(), output.length());
}

// Function to handle HTTP GET requests
void handle_get_request(int connfd, const std::string& request) {
    std::cout << "Handling GET request: " << request << std::endl;
    size_t question_mark = request.find('?');
    std::string path = request.substr(0, question_mark);
    std::string parameters = question_mark != std::string::npos ? request.substr(question_mark + 1) : "";

    std::string full_path = "data" + path; // Define your server's document root
    std::cout << "GET Path: " << full_path << std::endl;

    if (path.find(".cgi") != std::string::npos) {
        execute_cgi_script(connfd, full_path);
    } else {
        serve_file(connfd, full_path);
    }
}

// Function to handle client requests
void handle_request(int connfd) {
    char buffer[1024] = {0};
    read(connfd, buffer, 1024);
    std::string request(buffer);

    if (request.empty()) {
        std::string response = "HTTP/1.1 400 Bad Request\nContent-Type: text/plain\n\nBad Request: No data received";
        write(connfd, response.c_str(), response.length());
        close(connfd);
        return;
    }

    // Print the raw request
    std::cout << "Received request: " << request << std::endl;

    // Parse the request line
    std::istringstream req_stream(request);
    std::string method;
    std::string url;
    std::string version;
    req_stream >> method >> url >> version;

    std::cout << "Received url: " << url << std::endl;
    std::cout << "Received version: " << version << std::endl;

    if (method.empty() || url.empty() || version.empty()) {
        std::string response = "HTTP/1.1 400 Bad Request\nContent-Type: text/plain\n\nBad Request: Malformed request line";
        write(connfd, response.c_str(), response.length());
        close(connfd);
        return;
    }

    // Handle according to the method
    if (method == "GET") {
        handle_get_request(connfd, url);
    } else {
        std::string response = "HTTP/1.1 501 Not Implemented\nContent-Type: text/plain\n\nMethod not implemented";
        write(connfd, response.c_str(), response.length());
    }

    std::cout << "====== End of request ====== \n\n" << std::endl;
    close(connfd);

}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv <port>\n";
        return 1;
    }

    int port = atoi(argv[1]);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error binding socket\n";
        return 1;
    }

    if (listen(sockfd, 10) < 0) {
        std::cerr << "Error listening on socket\n";
        return 1;
    }

    std::cout << "Server is listening on http://" << inet_ntoa(addr.sin_addr) << ":" << port << std::endl;

    while (true) {
        int connfd = accept(sockfd, (struct sockaddr*)NULL, NULL);
        if (connfd < 0) {
            std::cerr << "Error accepting connection\n";
            continue;
        }

        // Fork a new process to handle the request
        if (fork() == 0) {
            close(sockfd);
            handle_request(connfd);
            exit(0);
        }
        close(connfd);
    }

    return 0;
}
