#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include<string.h>
class HttpClient {
public:
    HttpClient(const std::string& host, int port) : host_(host), port_(port) {}
    
    bool connect() {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            perror("socket");
            return false;
        }
        
        struct hostent* server = gethostbyname(host_.c_str());
        if (server == NULL) {
            std::cerr << "Error: no such host" << std::endl;
            return false;
        }
        
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        serv_addr.sin_port = htons(port_);
        
        if (::connect(sockfd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("connect");
            return false;
        }
        
        return true;
    }
    
    std::string sendRequest(const std::string& path = "/") {
        std::string request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host_ + ":" + std::to_string(port_) + "\r\n";
        request += "Connection: close\r\n\r\n";
        
        if (send(sockfd_, request.c_str(), request.length(), 0) < 0) {
            perror("send");
            return "";
        }
        
        char buffer[1024];
        std::string response;
        ssize_t n;
        
        while ((n = recv(sockfd_, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[n] = '\0';
            response += buffer;
        }
        
        return response;
    }
    
    ~HttpClient() {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
    }

private:
    std::string host_;
    int port_;
    int sockfd_ = -1;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }
    
    std::string host = argv[1];
    int port = std::stoi(argv[2]);
    
    HttpClient client(host, port);
    
    if (client.connect()) {
        std::string response = client.sendRequest();
        std::cout << "Server response:\n" << response << std::endl;
    } else {
        std::cout << "Failed to connect to server" << std::endl;
    }
    
    return 0;
}