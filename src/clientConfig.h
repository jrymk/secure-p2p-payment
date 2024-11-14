#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <fstream>
#include <string>

#define CLIENT_CONFIG_FILE "client.conf"

class ClientConfig {
public:
    std::string serverAddress = "localhost";
    std::string serverPort = "5050";
    std::string username;
    std::string p2pPort = "0";

    ClientConfig() {
        read();
    }

    void read() {
        username = "";
        p2pPort = "0";

        std::ifstream configFile(CLIENT_CONFIG_FILE);
        if (configFile.is_open()) {
            std::string line;
            while (std::getline(configFile, line)) {
                if (line.find("serverAddress=") != std::string::npos) {
                    serverAddress = line.substr(line.find("=") + 1);
                } else if (line.find("serverPort=") != std::string::npos) {
                    serverPort = line.substr(line.find("=") + 1);
                } else if (line.find("username=") != std::string::npos) {
                    username = line.substr(line.find("=") + 1);
                } else if (line.find("p2pPort=") != std::string::npos) {
                    p2pPort = line.substr(line.find("=") + 1);
                }
            }
            configFile.close();
        }
    }

    void write(bool rememberMe = false) {
        std::ofstream configFile(CLIENT_CONFIG_FILE);
        configFile << "serverAddress=" << serverAddress << std::endl;
        configFile << "serverPort=" << serverPort << std::endl;
        if (rememberMe && !username.empty()) {
            configFile << "username=" << username << std::endl;
            configFile << "p2pPort=" << p2pPort << std::endl;
        }
        configFile.close();
    }
};

#endif // CLIENT_CONFIG_H