#include "serverAction.h"
#include <string>

// args: <portNum> <Options>
// Options:
// -d: show client register, login, and exit info in console only
// -s: also show online list on login or exit
// -a: also show TCP messages, without this tag, errors will still be shown
// -h: run headless, no gui
int main(int argc, char *argv[]) {
    int consoleLogLevel = 0; // no log
    bool runHeadless = false;
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "-d")
            consoleLogLevel = std::max(consoleLogLevel, 1);
        else if (std::string(argv[i]) == "-s")
            consoleLogLevel = std::max(consoleLogLevel, 2);
        else if (std::string(argv[i]) == "-a")
            consoleLogLevel = std::max(consoleLogLevel, 3);
        else if (std::string(argv[i]) == "-h")
            runHeadless = true;
        else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            std::cerr << "args: <portNum> <Options>\nAvailable options:" << std::endl;
            std::cerr << "-d: show client register, login, and exit info in console only" << std::endl;
            std::cerr << "-s: also show online list on login or exit" << std::endl;
            std::cerr << "-a: also show TCP messages, without this tag, errors will still be shown" << std::endl;
            std::cerr << "-h: run headless, no gui" << std::endl;
            return 1;
        }
    }

    ServerAction serverAction(consoleLogLevel);
    if (!serverAction.startServer(argv[1])) {
        std::cerr << "Failed to start server: " << serverAction.error_t << std::endl;
        return 1;
    }
    serverAction.startListening();

    while (true) {
    }
}