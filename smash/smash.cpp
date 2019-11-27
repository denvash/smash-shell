#include <iostream>
#include <csignal>
#include "commands.h"
#include "signals.h"

int main(int argc, char *argv[]) {
    SmallShell &smash = SmallShell::getInstance();

    if (signal(SIGTSTP, ctrlZHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }

    while (true) {
        std::cout << "smash> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }

    return 0;
}