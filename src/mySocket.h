#ifndef SOCKET_H
#define SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sys/select.h>
#include <sys/time.h>
#include <cerrno>
#include <string>
#include <random>

// a custom simple socket class to consolidate the socket code
// heavily inspired by Beej's Guide to Network Programming
class MySocket {
public:
    int sockfd;
    std::string error_t;
    std::string socketNameForDebug = "Unknown";
    bool isConnected = false;

    MySocket(const std::string &socketName) : sockfd(-1), socketNameForDebug(socketName) {}

    int checkPort(const std::string &port) {
        if (port.empty())
            return -1;
        if (port.find_first_not_of("0123456789") != std::string::npos)
            return -1;
        int portNum;
        try {
            portNum = std::stoi(port);
        } catch (const std::exception &e) {
            error_t = "Failed to convert port to a number\nException: " + std::string(e.what());
            return -1;
        }
        if (portNum < 1024 || portNum > 65535)
            if (portNum != 0)
                return -1;
        return portNum;
    }

    bool connect(const std::string &hostname, const std::string &serverPort) {
        if (isConnected) {
            error_t = "Already connected to server";
            return false;
        }
        std::cerr << "SOCK " << socketNameForDebug << " connecting to " << hostname << ":" << serverPort << std::endl;
        struct addrinfo hints, *res, *server_info;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // fill in my IP for me
        if (getaddrinfo(hostname.c_str(), serverPort.c_str(), &hints, &res) != 0) {
            error_t = strerror(errno);
            std::cerr << "Failed to get address info" << std::endl;
            return false;
        }
        for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
            // only pick tcp
            if (p->ai_socktype != SOCK_STREAM)
                continue;
            server_info = p;
            break;
        }
        int sockfd = ::socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
        if (sockfd == -1) {
            error_t = strerror(errno);
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        if (::connect(sockfd, server_info->ai_addr, server_info->ai_addrlen) == -1) {
            error_t = strerror(errno);
            std::cerr << "Failed to connect to " << hostname << ":" << serverPort << std::endl;
            return false;
        }
        std::cerr << "Connected to " << hostname << ":" << serverPort << std::endl;

        freeaddrinfo(res);
        this->sockfd = sockfd;
        isConnected = true;
        std::cerr << "OK" << std::endl;
        return true;
    }

    bool bindSocket(const std::string &clientPort) {
        std::cerr << "SOCK " << socketNameForDebug << " binding to port " << clientPort << std::endl;
        struct addrinfo hints, *res, *p;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        if (getaddrinfo(NULL, clientPort.c_str(), &hints, &res) != 0) {
            error_t = strerror(errno);
            std::cerr << "Failed to get address info" << std::endl;
            return false;
        }
        for (p = res; p != NULL; p = p->ai_next) {
            sockfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sockfd == -1) {
                error_t = strerror(errno);
                continue;
            }
            if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                ::close(sockfd);
                error_t = strerror(errno);
                continue;
            }
            break;
        }
        freeaddrinfo(res);
        if (p == NULL) {
            error_t = "Failed to bind socket to port " + clientPort;
            return false;
        }
        std::cerr << "OK" << std::endl;
        return true;
    }

    bool listen(int timeout_sec = 5) {
        std::cerr << "SOCK " << socketNameForDebug << " starting to listen for connection" << std::endl;
        if (::listen(sockfd, 10) == -1) {
            error_t = strerror(errno);
            return false;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval tv;
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;

        int retval = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1) {
            error_t = strerror(errno);
            return false;
        } else if (retval == 0) {
            error_t = "Timeout occurred";
            return false;
        }

        std::cerr << "OK" << std::endl;
        return true;
    }

    void accept(MySocket &newSock) {
        std::cerr << "SOCK " << socketNameForDebug << " accepting connection" << std::endl;
        struct sockaddr_storage their_addr;
        socklen_t addr_size = sizeof(their_addr);
        newSock.sockfd = ::accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (newSock.sockfd == -1) {
            error_t = strerror(errno);
            return;
        }
        std::cerr << "OK" << std::endl;
    }

    int findAvailablePort() {
        std::cerr << "SOCK " << socketNameForDebug << " finding available port" << std::endl;
        int tempSockFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (tempSockFd == -1) {
            error_t = strerror(errno);
            return -1;
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1024, 65535);

        for (int tries = 0; tries < 10000; tries++) { // if it fails 10000 times, I think it deserves to fail
            int port = dis(gen);
            addr.sin_port = htons(port);
            if (::bind(tempSockFd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                ::close(tempSockFd);
                std::cerr << "OK Found available port: " << port << std::endl;
                return port;
            }
        }
        ::close(tempSockFd);
        return -1;
    }

    bool send(const std::string &message) {
        std::cerr << "SOCK " << socketNameForDebug << " sending: " << message << std::endl;
        int sendRes = ::send(sockfd, message.c_str(), message.size(), 0);
        if (sendRes == -1) {
            error_t = strerror(errno);
            return false;
        }
        return true;
    }

    std::string recv(int timeout_sec) {
        std::cerr << "SOCK " << socketNameForDebug << " receiving" << std::endl;
        char buf[1024];
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval tv;
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;

        int retval = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1) {
            error_t = strerror(errno);
            return "";
        } else if (retval == 0) {
            error_t = "Timeout occurred";
            return "";
        }

        int numbytes = ::recv(sockfd, buf, sizeof(buf) - 1, 0);
        std::cerr << "recv returned " << numbytes << std::endl;
        if (numbytes == -1) {
            error_t = strerror(errno);
            std::cerr << error_t << std::endl;
            return "";
        }
        if (numbytes == 0) {
            error_t = "Connection closed by peer";
            return "";
        }

        buf[numbytes] = '\0';
        std::cerr << "OK Received: " << buf << std::endl;
        return buf;
    }

    ~MySocket() {
        std::cerr << "SOCK " << " Closing " << socketNameForDebug << " socket" << std::endl;
        // close tcp connection
        if (isConnected) {
            ::shutdown(sockfd, SHUT_RDWR); // Gracefully shut down the connection
            ::close(sockfd);
        }
        std::cerr << "Connection closed gracefully" << std::endl;
    }
};

#endif // SOCKET_H