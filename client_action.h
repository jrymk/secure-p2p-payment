#ifndef CLIENT_ACTION_H
#define CLIENT_ACTION_H

#include "socket.h"
#include <vector>
#include <thread>
#include <atomic>

struct UserAccount {
    std::string username;
    std::string ipAddr;
    std::string p2pPort;
};

class ClientAction {
public:
    std::string serverAddress = "localhost";
    std::string port = "5050";

    MySocket clientSocket;

    std::string error_t;

    bool loggedIn = false;
    std::string username;
    std::string p2pPort;
    int accountBalance;
    std::string serverPublicKey;
    std::vector<UserAccount> userAccounts;
    std::function<void()> statusUpdatedCallback;

    // p2p
    MySocket p2pListenSocket;

    std::thread listeningThread;
    std::atomic<bool> p2pListening;

    ClientAction() : clientSocket("client"), p2pListenSocket("p2pListen") {}

    bool connectToServer(const std::string &hostname, const std::string &serverPort) {
        clientSocket.connect(hostname, serverPort);
        if (!clientSocket.isConnected)
            error_t = "Please check server configuration from the login window.\nServer address: " + hostname + "\nPort: " + serverPort + "\nError: " + clientSocket.error_t;
        if (clientSocket.isConnected) {
            serverAddress = hostname;
            port = serverPort;
        }
        return clientSocket.isConnected;
    }

    bool registerAccount(const std::string &username) {
        if (!clientSocket.isConnected) {
            error_t = "Not connected to server";
            return false;
        }
        clientSocket.send("REGISTER#" + username);
        std::string response = clientSocket.recv(5);

        if (response.substr(0, 3) == "100") {
            error_t = "Server response: " + response;
            return true;
        }
        error_t = "Server response: " + response + "\nPerhaps the username is already taken.";
        return false;
    }

    bool logIn(const std::string &username, const std::string &p2pPort) {
        if (!clientSocket.isConnected) {
            error_t = "Not connected to server";
            return false;
        }
        if (username.empty()) {
            error_t = "Username cannot be empty";
            return false;
        }
        this->username = username;

        if (p2pPort.empty()) {
            error_t = "Client port cannot be empty";
            return false;
        }
        if (p2pPort.find_first_not_of("0123456789") != std::string::npos) {
            error_t = "Invalid client port";
            return false;
        }

        int portNum = p2pListenSocket.checkPort(p2pPort);
        if (portNum == -1) {
            error_t = "Invalid client port";
            return false;
        }
        if (portNum == 0) {
            // auto port
            int findPort = p2pListenSocket.findAvailablePort();
            if (findPort == -1) {
                error_t = "Failed to find an available port";
                return false;
            }
            this->p2pPort = std::to_string(findPort);
        } else {
            this->p2pPort = std::to_string(portNum);
        }

        clientSocket.send(this->username + "#" + this->p2pPort);
        std::string response = clientSocket.recv(5);

        if (response.substr(0, 3) == "220") {
            error_t = "Please check your username and try again.\nServer response: " + response;
            return false;
        }

        if (!p2pListenSocket.bindSocket(this->p2pPort)) {
            error_t = "Failed to bind p2p port. Please check if the port is available on your system.\n" + p2pListenSocket.error_t;
            return false;
        }

        p2pStartListening();

        if (!parseOnlineUsers(response)) {
            error_t = "Log in successful but failed to parse online users\n" + error_t;
            return false;
        }

        return true;
    }

    bool parseOnlineUsers(const std::string &response) {
        /* format:
        <accountBalance><CRLF>
        <serverPublicKey><CRLF>
        <number of accounts online><CRLF>
        <userAccount1>#<userAccount1_IPaddr>#<userAccount1_portNum><CRLF>
        <userAccount2>#<userAccount2_ IPaddr>#<userAccount2_portNum><CRLF>
        */

        userAccounts.clear();

        try {
            std::vector<std::string> lines;
            std::string line;
            std::istringstream responseStream(response);
            while (std::getline(responseStream, line)) {
                lines.push_back(line);
            }

            if (lines.size() < 4) {
                error_t = "Invalid response";
                return false;
            }

            accountBalance = std::stoi(lines[0]);
            serverPublicKey = lines[1];

            int numAccounts = std::stoi(lines[2]);

            for (int i = 3; i < numAccounts + 3; i++) {
                std::istringstream accountStream(lines[i]);
                std::string username, ipAddr, portNum;
                std::getline(accountStream, username, '#');
                std::getline(accountStream, ipAddr, '#');
                std::getline(accountStream, portNum);
                userAccounts.push_back({username, ipAddr, portNum});
            }
        } catch (const std::exception &e) {
            error_t = "Server response: " + response + "\nException: " + e.what();
            return false;
        }

        return true;
    }

    bool fetchServerInfo() {
        if (!clientSocket.isConnected) {
            error_t = "Not connected to server";
            return false;
        }
        clientSocket.send("List");
        bool parseSuccess = parseOnlineUsers(clientSocket.recv(5));

        statusUpdatedCallback();

        return parseSuccess;
    }

    bool sendMicropaymentTransaction(int amount, const std::string &payeeUsername) {
        // format: <MyUserAccountName>#<payAmount>#<PayeeUserAccountName>
        if (!clientSocket.isConnected) {
            error_t = "Not connected to server";
            return false;
        }

        // find the payee's IP address and port number
        std::string payeeIPAddr = "";
        std::string payeePort = "";
        for (const auto &user : userAccounts) {
            if (user.username == payeeUsername) {
                payeeIPAddr = user.ipAddr;
                payeePort = user.p2pPort;
                break;
            }
        }

        if (payeeIPAddr.empty() || payeePort.empty()) {
            error_t = "Payee IP address or port number not found in the list of online users";
            return false;
        }

        MySocket p2pSendSocket("p2pSend");
        if (!p2pSendSocket.connect(payeeIPAddr, payeePort)) {
            error_t = "Failed to connect to payee\n" + p2pSendSocket.error_t;
            return false;
        }

        p2pSendSocket.send(username + "#" + std::to_string(amount) + "#" + payeeUsername);
        std::cerr << "Sent micropayment transaction to " << payeeUsername << std::endl;

        return true;
    }

    ~ClientAction() {
        if (p2pListening)
            p2pStopListening();

        if (clientSocket.isConnected) {
            clientSocket.send("Exit"); // send exit message to server on connection close
            std::cout << "Server replied: " << clientSocket.recv(5) << std::endl;
            std::cout << clientSocket.error_t << std::endl;
            // if ( == "Bye\r\n")
            //     std::cerr << "Server closed connection" << std::endl;
        }
    }

    void p2pStartListening() {
        p2pListening = true;
        listeningThread = std::thread([this]() {
            while (p2pListening) {
                if (p2pListenSocket.listen(1)) {
                    MySocket p2pRecv("p2p_recv");
                    p2pListenSocket.accept(p2pRecv);
                    std::string message = p2pRecv.recv(1);
                    if (message.empty())
                        error_t = "Failed to receive message from peer\n" + p2pRecv.error_t;
                    else
                        handleIncomingMessage(message);
                } else {
                    error_t = "Failed to listen for incoming connections (timeout)\n" + p2pListenSocket.error_t;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void p2pStopListening() {
        p2pListening = false;
        std::cerr << "Stopping p2p listening thread" << std::endl;
        if (listeningThread.joinable())
            listeningThread.join();
        std::cerr << "Stopped p2p listening thread" << std::endl;
    }

    void handleIncomingMessage(const std::string &message) {
        // Process the incoming message
        std::cout << "Received message: " << message << std::endl;

        // format: <senderUsername>#<amount>#<receiverUsername>
        std::istringstream messageStream(message);
        std::string payerUsername, amount, payeeUsername;
        std::getline(messageStream, payerUsername, '#');
        std::getline(messageStream, amount, '#');
        std::getline(messageStream, payeeUsername);

        std::cout << "Payer: " << payerUsername << ", Amount: " << amount << ", Payee: " << payeeUsername << std::endl;

        clientSocket.send(payerUsername + "#" + amount + "#" + payeeUsername);

        // fetchServerInfo(); // I don't think we can do this here, because multithreading thing
    }

    void quitApp() {
        if (p2pListening) {
            std::cerr << "Waiting for p2p listening thread to stop" << std::endl;
            p2pStopListening();
        }
        std::cerr << "App quitted" << std::endl;
    }
};

ClientAction clientAction;

#endif // CLIENT_ACTION_H