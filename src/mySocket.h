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
#include <fcntl.h>
#include <sys/time.h>
#include <cerrno>
#include <string>
#include <random>
#include <sstream>
#include "encryption.h"
#include <sstream>
// a custom simple socket class to consolidate the socket code
// heavily inspired by Beej's Guide to Network Programming
class MySocket {
public:
    int sockfd;
    std::string error_t;
    std::string socketNameForDebug = "Unknown";
    bool enableLogging = false;
    bool isConnected = false;

    MySocket(const std::string &socketName, bool enableLogging = false) : sockfd(-1), socketNameForDebug(socketName), enableLogging(enableLogging) {}

    // checks if a string port number is valid, and converts it to an integer. isServer = true allows ports less than 1024
    int checkPort(const std::string &port, bool isServer = false) {
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
        if (isServer)
            return portNum;
        if (portNum < 1024 || portNum > 65535)
            if (portNum != 0)
                return -1;
        return portNum;
    }

    // connect to the given hostname/IP address and port using TCP
    bool connect(const std::string &hostname, const std::string &serverPort, int timeout = 5) {
        if (isConnected) {
            error_t = "Already connected to server";
            return false;
        }
        if (enableLogging)
            std::cerr << "Socket " << socketNameForDebug << " connecting to " << hostname << ":" << serverPort << std::endl;
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

        // Set socket to non-blocking
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | SOCK_NONBLOCK);

        int result = ::connect(sockfd, server_info->ai_addr, server_info->ai_addrlen);
        if (result == -1 && errno != EINPROGRESS) {
            error_t = strerror(errno);
            std::cerr << "Failed to connect to " << hostname << ":" << serverPort << std::endl;
            return false;
        }

        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sockfd, &writefds);

        struct timeval tv;
        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        result = select(sockfd + 1, NULL, &writefds, NULL, &tv);
        if (result == -1) {
            error_t = strerror(errno);
            std::cerr << "Select error" << std::endl;
            return false;
        } else if (result == 0) {
            error_t = "Connection timed out";
            std::cerr << "Connection timed out" << std::endl;
            return false;
        }

        int so_error;
        socklen_t len = sizeof so_error;
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            error_t = strerror(so_error);
            std::cerr << "Connection failed: " << error_t << std::endl;
            return false;
        }

        // Set socket back to blocking mode
        fcntl(sockfd, F_SETFL, flags);

        freeaddrinfo(res);
        this->sockfd = sockfd;
        isConnected = true;
        if (enableLogging)
            std::cerr << "Connected to " << hostname << ":" << serverPort << std::endl;
        return true;
    }

    // bind the socket to the given port on the system
    bool bindSocket(const std::string &clientPort) {
        if (enableLogging)
            std::cerr << "Socket " << socketNameForDebug << " binding to port " << clientPort << std::endl;
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
        if (enableLogging)
            std::cerr << "OK" << std::endl;
        return true;
    }

    // listen for incoming TCP connections
    bool listen(int timeout_sec = 5) {
        // std::cerr << "Socket " << socketNameForDebug << " starting to listen for connection" << std::endl;
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

        if (enableLogging)
            std::cerr << "Listen OK" << std::endl;
        return true;
    }

    // accept the incoming connection with the given socket
    std::pair<std::string, std::string> accept(MySocket &newSock) {
        if (enableLogging)
            std::cerr << "Socket " << socketNameForDebug << " accepting connection" << std::endl;
        struct sockaddr_storage their_addr;
        socklen_t addr_size = sizeof(their_addr);
        newSock.sockfd = ::accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (newSock.sockfd == -1) {
            error_t = strerror(errno);
            return {"", ""};
        }
        if (enableLogging)
            std::cerr << "OK" << std::endl;
        // return ipv4 address and port
        char ipstr[INET_ADDRSTRLEN];
        struct sockaddr_in *s = (struct sockaddr_in *)&their_addr;
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr));
        return {ipstr, std::to_string(ntohs(s->sin_port))};
    }

    // randomly find an available port on the system
    int findAvailablePort() {
        if (enableLogging)
            std::cerr << "Socket " << socketNameForDebug << " finding available port" << std::endl;
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
                if (enableLogging)
                    std::cerr << "OK Found available port: " << port << std::endl;
                return port;
            }
        }
        ::close(tempSockFd);
        return -1;
    }

    // send a message with the socket
    bool send(const std::string &message) {
        if (enableLogging)
            std::cerr << "Socket " << socketNameForDebug << " sending: " << message << std::endl;
        int sendRes = ::send(sockfd, message.c_str(), message.size(), 0);
        if (sendRes == -1) {
            error_t = strerror(errno);
            return false;
        }
        return true;
    }

    // receive a message from the socket
    std::string recv(int timeout_sec = 5) {
        if (enableLogging)
            std::cerr << "Socket " << socketNameForDebug << " receiving" << std::endl;
        char buf[4096];
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

        if (!FD_ISSET(sockfd, &readfds)) {
            error_t = "Socket not ready for reading";
            std::cerr << "FD_ISSET error: " << error_t << std::endl;
            return "";
        }

        int numbytes = ::recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (enableLogging)
            std::cerr << "recv returned " << numbytes << std::endl;
        if (numbytes == -1) {
            error_t = strerror(errno);
            std::cerr << "ERROR: " << error_t << std::endl;
            return "";
        }
        if (numbytes == 0) {
            error_t = "Connection closed by peer";
            return "";
        }

        buf[numbytes] = '\0';
        if (enableLogging)
            std::cerr << "OK Received: " << buf << std::endl;
        return buf;
    }

    bool sendEncrypted(EVP_PKEY *publicKey, const std::string &message) {
        std::string output = "------ ENCRYPTED ------\r\n";
        for (int i = 0; i < message.size(); i += 202) {
            output += encryptMessage(publicKey, message.substr(i, 202)) + "\r\n";
        }
        output += "------ END ------\r\n";

        return send(output);
    }

    std::string recvEncrypted(EVP_PKEY *privateKey, bool *encrypted = nullptr) {
        std::string raw = recv(5);
        // print the raw message with \r \n and other special characters printed out
        if (enableLogging) {
            std::cerr << "Raw message: ";
            for (char c : raw) {
                if (c == '\r')
                    std::cerr << "\\r";
                else if (c == '\n')
                    std::cerr << "\\n";
                else
                    std::cerr << c;
            }
            std::cerr << std::endl;
        }

        if (raw.empty())
            return raw;
        if (raw.substr(0, 23) != "------ ENCRYPTED ------") {
            std::cerr << "Header not encrypted" << std::endl;
            if (encrypted)
                *encrypted = false;
            return raw;
        }
        std::stringstream ss(raw);
        std::string line;
        std::string message;
        bool reading = false;
        while (std::getline(ss, line, '\r')) {
            if (line.empty())
                continue;
            if (line[0] == '\n')
                line = line.substr(1);
            if (line == "------ ENCRYPTED ------") {
                reading = true;
                continue;
            }
            if (line == "------ END ------") {
                reading = false;
                break;
            }
            if (reading) {
                std::string decrypted = decryptMessage(privateKey, line);
                if (decrypted.empty()) {
                    if (encrypted)
                        *encrypted = false;
                    error_t = "Failed to decrypt message";
                    return raw;
                }
                message += decrypted;
            }
        }
        if (reading) {
            if (encrypted)
                *encrypted = false;
            error_t = "Invalid encrypted message format";
            return raw;
        }
        if (encrypted)
            *encrypted = true;

        std::cerr << "Decrypted message: " << message << std::endl;

        return message;
    }

    // close the active TCP connection. This is also called when the MySocket object is destroyed
    void closeConnection() {
        if (enableLogging)
            std::cerr << "Socket " << " Closing " << socketNameForDebug << " socket" << std::endl;
        // close tcp connection
        if (isConnected) {
            ::shutdown(sockfd, SHUT_RDWR); // Gracefully shut down the connection
            ::close(sockfd);
            isConnected = false;
        }
        if (enableLogging)
            std::cerr << "Connection closed gracefully" << std::endl;
    }

    ~MySocket() {
        if (isConnected)
            closeConnection();
    }
};

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

#endif // SOCKET_H