#ifndef CLIENT_ACTION_H
#define CLIENT_ACTION_H

#include "clientConfig.h"
#include <vector>
#include <thread>
#include <atomic>
#include "mySocket.h"
#include "encryption.h"

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

    ClientConfig clientConfig;
    bool loggedIn = false;

    std::string username;
    std::string p2pPort;
    int accountBalance;
    std::vector<UserAccount> userAccounts;
    bool transferOk = false;
    std::function<void()> statusUpdatedCallback;
    std::function<void()> sessionEndedCallback;

    bool waitingForRecv = false;

    EVP_PKEY *serverPublicKey = nullptr;
    EVP_PKEY *clientPrivateKey = nullptr;

    // p2p
    MySocket p2pListenSocket;

    std::thread listeningThread;
    std::atomic<bool> p2pListening;

    ClientAction() : clientSocket("client", true), p2pListenSocket("p2pListen") {
        checkKeyFiles();
        clientPrivateKey = stringToKey(loadKeyFromFile(PRIVATE_KEY_FILE), true);
    }

    bool connectToServer(const std::string &hostname, const std::string &serverPort) {
        std::cerr << "Connecting to server" << std::endl;
        // add timeout
        clientSocket.connect(hostname, serverPort, 5);
        if (!clientSocket.isConnected)
            error_t = "Please check server configuration from the login window.\nServer address: " + hostname + "\nPort: " + serverPort + "\nError: " + clientSocket.error_t;
        if (clientSocket.isConnected) {
            serverAddress = hostname;
            port = serverPort;
        }

        clientSocket.send("HELLO");
        std::string response = clientSocket.recv(5);

        std::cerr << "Public key received, reading" << std::endl;

        serverPublicKey = stringToKey(response, false);
        std::cerr << "Server public key: " << response.substr(27, 37) << "..." << std::endl;

        return clientSocket.isConnected;
    }

    bool registerAccount(const std::string &username) {
        std::cerr << "Registering account" << std::endl;
        if (!clientSocket.isConnected) {
            error_t = "Not connected to server";
            return false;
        }
        clientSocket.sendEncrypted(serverPublicKey, "REGISTER#" + username);
        std::string response = clientSocket.recvEncrypted(clientPrivateKey);

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

        std::string publicKey = loadKeyFromFile(PUBLIC_KEY_FILE);

        clientSocket.sendEncrypted(serverPublicKey, "LOGIN#" + this->username + "#" + this->p2pPort + "#" + publicKey);
        std::string response = clientSocket.recvEncrypted(clientPrivateKey);

        if (response.substr(0, 13) == "220 AUTH_FAIL") {
            error_t = "Please check your username and try again.\nServer response: " + response;
            return false;
        }
        if (response.substr(0, 17) == "250 MESSAGE_ERROR") {
            error_t = "Error message format from client.\nServer response: " + response;
            return false;
        }

        loggedIn = true;

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
        std::cerr << "Parsing online users" << std::endl;

        try {
            std::vector<std::string> lines;
            std::string line;
            std::istringstream responseStream(response);
            while (std::getline(responseStream, line)) {
                // remove trailing \r
                if (!line.empty() && line[line.size() - 1] == '\r')
                    line.pop_back();
                lines.push_back(line);
            }

            std::cerr << "Received " << lines.size() << " lines" << std::endl;

            if (lines.size() && lines[0] == "Please login first") {
                error_t = "Not logged in or session ended by the server. Response: Please login first";
                sessionEndedCallback();
                return false;
            }

            for (const auto &line : lines) {
                if ("Transfer OK!")
                    transferOk = true;
            }

            if (lines.size() < 3) {
                error_t = "Invalid response";
                return false;
            }

            accountBalance = std::stoi(lines[0]);

            int numAccounts = std::stoi(lines[1]);

            userAccounts.clear();
            for (int i = 2; i < numAccounts + 2; i++) {
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
        // don't let the List request interfere with Transfer OK! response, etc.
        if (waitingForRecv) {
            std::cerr << "Waiting for Transfer OK! receive/timeout" << std::endl;
            return false;
        }
        clientSocket.sendEncrypted(serverPublicKey, "List");
        std::string response = clientSocket.recvEncrypted(clientPrivateKey);
        bool parseSuccess = parseOnlineUsers(response);

        if (!parseSuccess) {
            error_t = "Failed to fetch server info\n" + error_t;
            std::cerr << error_t << std::endl;
        }

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
                std::cerr << payeePort.size() << std::endl;
                break;
            }
        }

        // find the payee's public key
        clientSocket.sendEncrypted(serverPublicKey, "PKEY#" + payeeUsername);
        std::string payeePkey = clientSocket.recvEncrypted(clientPrivateKey);
        if (payeePkey.empty() || payeePkey.substr(0, 3) == "240") {
            error_t = "Failed to fetch payee's public key\n" + clientSocket.error_t;
            return false;
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

        transferOk = false;

        waitingForRecv = true;
        if (p2pSendSocket.sendEncrypted(stringToKey(payeePkey, false), username + "#" + std::to_string(amount) + "#" + payeeUsername)) {
            std::cerr << "Sent micropayment transaction to " << payeeUsername << std::endl;
            return true;
        }

        error_t = "Failed to send payment to " + payeeUsername + "\nError: " + error_t;
        return false;
    }

    bool verifyMicropaymentTransaction() {
        std::string response = clientSocket.recvEncrypted(clientPrivateKey);
        waitingForRecv = false;
        if (response == "Transfer OK!\n" || response == "Transfer OK!\r\n") {
            transferOk = true;
            return true;
        }
        error_t = "Server response: " + response;
        return false;
    }

    void logOut() {
        if (clientSocket.isConnected) {
            clientSocket.sendEncrypted(serverPublicKey, "Exit");
            std::cout << "Server replied: " << clientSocket.recvEncrypted(clientPrivateKey) << std::endl;
            std::cout << clientSocket.error_t << std::endl;
        }
        loggedIn = false;
        if (p2pListening)
            p2pStopListening();
        clientSocket.closeConnection();
    }

    ~ClientAction() {
        logOut();
    }

    void p2pStartListening() {
        p2pListening = true;
        listeningThread = std::thread([this]() {
            while (p2pListening) {
                if (p2pListenSocket.listen(1)) {
                    MySocket p2pRecv("p2p_recv");
                    p2pListenSocket.accept(p2pRecv);
                    bool encrypted = false;
                    std::string message = p2pRecv.recvEncrypted(clientPrivateKey, &encrypted);
                    if (message.empty())
                        error_t = "Failed to receive message from peer\n" + p2pRecv.error_t;
                    else if (!encrypted)
                        error_t = "Received unencrypted message from peer";
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

        clientSocket.sendEncrypted(serverPublicKey, payerUsername + "#" + amount + "#" + payeeUsername);

        // fetchServerInfo(); // I don't think we can do this here, because multithreading thing
    }

    void quitApp() {
        p2pListenSocket.closeConnection();

        if (p2pListening) {
            std::cerr << "Waiting for p2p listening thread to stop" << std::endl;
            p2pStopListening();
        }
        std::cerr << "App quitted" << std::endl;
    }
};

ClientAction clientAction;

#endif // CLIENT_ACTION_H