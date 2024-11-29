#ifndef SERVER_ACTION_H
#define SERVER_ACTION_H

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <iomanip>
#include "mySocket.h"

struct UserAccount {
    std::string username;
    int balance;
};

struct OnlineEntry {
    MySocket *clientSocket;
    std::string username;
    std::string ipAddr;
    int clientPort;
    int p2pPort;
};

class ServerAction {
public:
    int consoleLogLevel = 0;

    std::vector<UserAccount> userAccounts;
    std::vector<OnlineEntry> onlineUsers;

    MySocket serverSocket;
    std::string error_t;

    std::atomic<bool> serverListening;
    std::thread listeningThread;

    ServerAction() : serverSocket("server") {
    }

    bool startServer(const std::string &port) {
        int portNum = serverSocket.checkPort(port, true);
        if (portNum == -1) {
            error_t = "Invalid port number";
            return false;
        }

        if (!serverSocket.bindSocket(port)) {
            error_t = "Failed to bind to port " + port + "\n" + serverSocket.error_t;
            return false;
        }

        if (consoleLogLevel >= 3)
            std::cerr << "Server started on port " << port << std::endl;
        return true;
    }

    void startListening() {
        serverListening = true;
        listeningThread = std::thread([this]() {
            while (serverListening) {
                if (serverSocket.listen(1)) {
                    MySocket *client = new MySocket("client" + std::to_string(onlineUsers.size()), consoleLogLevel >= 3);
                    auto ipAndPort = serverSocket.accept(*client);
                    if (ipAndPort.first.empty()) {
                        error_t = "Failed to accept incoming connection\n" + serverSocket.error_t;
                        continue;
                    }
                    std::cout << "\033[35;1mAccepted connection from " << ipAndPort.first << ":" << ipAndPort.second << "\033[0m" << std::endl;
                    onlineUsers.emplace_back(OnlineEntry{client, "", ipAndPort.first, serverSocket.checkPort(ipAndPort.second), 0});

                    std::thread([this, ipAndPort]() { clientInstance(ipAndPort); }).detach();
                }
            }
        });
    }

    void clientInstance(const std::pair<std::string, std::string> &clientAddr) {
        while (true) {
            std::vector<OnlineEntry>::iterator clientEntry = findOnlineUser(clientAddr);
            if (clientEntry == onlineUsers.end()) {
                std::cerr << "\033[31mClient " << clientAddr.first << ":" << clientAddr.second << " not found in online list" << "\033[0m" << std::endl;
                break;
            }

            if (!handleIncomingMessage(clientEntry))
                break;

            // std::string message = clientEntry->clientSocket.recv(5);
            // if (message.empty()) {
            //     // check if socket is still connected
            //     if (clientEntry->clientSocket.error_t == "Connection closed by peer") {
            //         std::cerr << "\033[31mClient " << clientEntry->ipAddr << ":" << clientEntry->clientPort << " disconnected" << "\033[0m" << std::endl;
            //         break;
            //     } else if (clientEntry->clientSocket.error_t == "Timeout occurred") {
            //         std::cerr << "\033[31mClient " << clientEntry->ipAddr << ":" << clientEntry->clientPort << " disconnected (listen timeout)" << "\033[0m" << std::endl;
            //     }
            // }
            // std::cerr << "Received message: " << message << std::endl;

            // if (message == "Exit")
            //     clientEntry->clientSocket.send("Bye\r\n");
            // sleep 100ms
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (consoleLogLevel >= 3)
            std::cerr << "Connection closed" << std::endl;
    }

    std::vector<OnlineEntry>::iterator findOnlineUser(const std::pair<std::string, std::string> &ipAndPort) {
        for (auto user = onlineUsers.begin(); user != onlineUsers.end(); user++) {
            int port = serverSocket.checkPort(ipAndPort.second);
            if (user->ipAddr == ipAndPort.first && user->clientPort == port) {
                return user;
            }
        }
        return onlineUsers.end();
    }

    std::vector<UserAccount>::iterator findUserAccount(const std::string &username) {
        for (auto user = userAccounts.begin(); user != userAccounts.end(); user++) {
            if (user->username == username) {
                return user;
            }
        }
        return userAccounts.end();
    }

    bool handleIncomingMessage(std::vector<OnlineEntry>::iterator &clientEntry) {
        MySocket *client = clientEntry->clientSocket;
        std::pair<std::string, std::string> ipAndPort = {clientEntry->ipAddr, std::to_string(clientEntry->clientPort)};

        if (consoleLogLevel >= 3)
            std::cerr << "Waiting for message from " << ipAndPort.first << ":" << ipAndPort.second << std::endl;
        std::string message = client->recv(5);
        if (consoleLogLevel >= 3)
            std::cerr << "Received message: " << message << std::endl;

        if (message.empty()) {
            // check if socket is still connected
            if (client->error_t == "Connection closed by peer") {
                std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " disconnected" << "\033[0m" << std::endl;
                // erase the client from the online list
                onlineUsers.erase(clientEntry);
                return false; // end the client thread
            } else if (client->error_t == "Timeout occurred") {
                std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " disconnected (listen timeout)" << "\033[0m" << std::endl;
            }
            return true;
        }

        // register:
        // REGISTER#<UserAccountName>
        // login:
        // <UserAccountName>#<portNum>
        // list online users:
        // List
        // logout:
        // Exit
        // micropayment transfer:s
        // <MyUserAccountName>#<payAmount>#<PayeeUserAccountName>

        std::vector<std::string> parts = split(message, '#');
        if (parts.size() < 1) {
            error_t = "Invalid message format";
            return true;
        }

        // register
        if (parts[0] == "REGISTER") {
            if (parts.size() != 2) {
                error_t = "Invalid message format";
                std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " failed to register username, " << error_t << "\033[0m" << std::endl;
                return true;
            }
            if (registerUser(*client, parts[1])) {
                if (consoleLogLevel >= 1) {
                    std::cout << "\033[36;1mClient " << ipAndPort.first << ":" << ipAndPort.second
                              << " registered username " << parts[1] << "\033[0m" << std::endl;
                    if (consoleLogLevel >= 2) {
                        printOnlineList();
                    }
                }
            } else
                std::cerr << "\033[31mClient " << ipAndPort.first << ":" << "\033[0m" << ipAndPort.second
                          << " failed to register username " << parts[1] << ", " << error_t << std::endl;
            return true;
        } else if (parts[0] == "List") {
            auto onlineUser = findOnlineUser(ipAndPort);
            if (onlineUser == onlineUsers.end()) {
                client->send("Please log in first\r\n");
                std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " requested online list but not found in online list" << "\033[0m" << std::endl;
                return true;
            }
            auto userAccount = findUserAccount(onlineUser->username);
            if (userAccount == userAccounts.end()) {
                client->send("Please register first\r\n");
                std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " requested online list but not found in user accounts" << "\033[0m" << std::endl;
                return true;
            }
            sendOnlineUsers(*client, *userAccount);
        } else if (parts[0] == "Exit") {
            // logout
            if (parts.size() != 1) {
                error_t = "Invalid message format";
                return true;
            }
            client->send("Bye\r\n");
            if (consoleLogLevel >= 3)
                std::cerr << "\033[34mClient " << ipAndPort.first << ":" << ipAndPort.second << " logged out" << "\033[0m" << std::endl;
            std::string username;
            auto onlineUser = findOnlineUser(ipAndPort);
            if (onlineUser == onlineUsers.end()) {
                std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " logged out but not found in online list" << "\033[0m" << std::endl;
                username = "unknown";
            } else {
                username = onlineUser->username;
                if (username.empty()) {
                    username = "<guest>";
                }
            }
            if (consoleLogLevel >= 1) {
                std::cout << "\033[34;1mClient " << username << " logged out" << "\033[0m" << std::endl;
                if (consoleLogLevel >= 2) {
                    printOnlineList();
                }
            }

            onlineUsers.erase(onlineUser);
            return false;
        } else { // no keywords
            if (parts.size() == 2) {
                // I hope it is a login
                auto userAccount = findUserAccount(parts[0]);
                if (userAccount == userAccounts.end()) {
                    client->send("220 AUTH FAIL\r\n");
                    std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " failed to log in, user not found" << "\033[0m" << std::endl;
                    return true;
                }
                auto clientOnline = findOnlineUser(ipAndPort);
                if (clientOnline == onlineUsers.end()) {
                    client->send("230 SERVER ERROR\r\n");
                    std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " socket connection not found" << "\033[0m" << std::endl;
                    return true;
                }

                // check other log in sessions and sign them out
                for (auto user = onlineUsers.begin(); user != onlineUsers.end(); user++) {
                    if (user->username == parts[0]) {
                        user->username = "";
                        user->p2pPort = 0;
                        break;
                    }
                }

                clientOnline->username = parts[0];
                clientOnline->p2pPort = clientOnline->clientSocket->checkPort(parts[1]);

                sendOnlineUsers(*client, *userAccount);

                if (consoleLogLevel >= 1) {
                    std::cout << "\033[32;1mClient " << ipAndPort.first << ":" << ipAndPort.second
                              << " logged in as " << parts[0] << "\033[0m" << std::endl;
                    if (consoleLogLevel >= 2) {
                        printOnlineList();
                    }
                }
            } else if (parts.size() == 3) {
                // I hope it is a micropayment transfer
                auto payer = findUserAccount(parts[0]);
                auto payee = findUserAccount(parts[2]);
                if (payer == userAccounts.end()) {
                    std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " failed to transfer micropayment, payer not found" << "\033[0m" << std::endl;
                    return true;
                }
                if (payee == userAccounts.end()) {
                    std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " failed to transfer micropayment, payee not found" << "\033[0m" << std::endl;
                    return true;
                }
                // check online
                auto payeeOnline = findOnlineUser({ipAndPort.first, ipAndPort.second});
                if (payeeOnline == onlineUsers.end() || payeeOnline->username != parts[2]) {
                    std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " failed to transfer micropayment, payee not online, or message not from payee" << "\033[0m" << std::endl;
                    return true;
                }
                auto payerOnline = std::vector<OnlineEntry>::iterator();
                for (auto user = onlineUsers.begin(); user != onlineUsers.end(); user++) {
                    if (user->username == parts[0]) {
                        payerOnline = user;
                        break;
                    }
                }
                if (payerOnline == onlineUsers.end()) {
                    std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " failed to transfer micropayment, payer not online" << "\033[0m" << std::endl;
                }
                try {
                    int amount = std::stoi(parts[1]);
                } catch (const std::exception &e) {
                    std::cerr << "\033[31mClient " << ipAndPort.first << ":" << ipAndPort.second << " failed to transfer micropayment, failed to convert " << parts[1] << " to integer." << "\033[0m" << std::endl;
                    return true;
                }

                // transfer the amount
                payer->balance -= std::stoi(parts[1]);
                payee->balance += std::stoi(parts[1]);

                // send confirmation to payer
                payerOnline->clientSocket->send("Transfer OK!\r\n");
            }
        }
        return true;
    }

    void printOnlineList() {
        std::cerr << "\033[33;7mOnline users list:\033[27m" << std::endl;
        std::cerr << std::right << std::setw(20) << "Username  "
                  << std::left << std::setw(16) << "IP Address"
                  << std::setw(6) << "Port"
                  << std::setw(6) << "P2P Port" << std::endl;
        for (const auto &user : onlineUsers) {
            std::cerr << std::right << std::setw(20) << user.username + "  "
                      << std::left << std::setw(16) << user.ipAddr
                      << std::setw(6) << user.clientPort
                      << std::setw(6) << user.p2pPort << std::endl;
        }
        std::cerr << "\033[0m";
    }

    bool registerUser(MySocket &client, const std::string &username) {
        for (auto &user : userAccounts) {
            if (user.username == username) {
                client.send("210 FAIL\r\n");
                error_t = "User already exists";
                return false;
            }
        }
        UserAccount newUser;
        newUser.username = username;
        newUser.balance = 10000;
        userAccounts.emplace_back(newUser);
        client.send("100 OK\r\n");
        return true;
    }

    bool sendOnlineUsers(MySocket &client, const UserAccount &record) {
        std::string response = std::to_string(record.balance) + "\r\n";
        response += "public key\r\n";

        std::vector<OnlineEntry> filteredOnlineUsers;
        for (const auto &onlineUser : onlineUsers) {
            if (onlineUser.username.empty())
                continue;
            filteredOnlineUsers.emplace_back(onlineUser);
        }

        response += std::to_string(filteredOnlineUsers.size()) + "\r\n";
        for (const auto &onlineUser : filteredOnlineUsers) {
            response += onlineUser.username + "#" + onlineUser.ipAddr + "#" + std::to_string(onlineUser.p2pPort) + "\r\n";
        }
        if (client.send(response)) {
            if (consoleLogLevel >= 3)
                std::cerr << "Sent online users list to " << record.username << std::endl;
            return true;
        } else {
            error_t = "Failed to send online users list to " + record.username + "\n" + client.error_t;
            return false;
        }
        return true;
    }

    void stopListening() {
        serverListening = false;
        if (consoleLogLevel >= 3)
            std::cerr << "Stopping server listening thread" << std::endl;
        if (listeningThread.joinable())
            listeningThread.join();
        std::cerr << "\033[7mServer stopped successfully\033[0m" << std::endl;
    }
};

ServerAction serverAction;

#endif // SERVER_ACTION_H