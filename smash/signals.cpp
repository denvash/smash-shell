#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include "commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    auto fgPid = SmallShell::fgPid;

    // Don't do anything if no fg process
    if (fgPid == -1) {
        return;
    }

    auto killRes = kill(fgPid, SIGKILL);

    if (killRes == -1) {
        logErrorSystemCall("kill");
    } else {
        cout << "smash: process " << fgPid << " was killed" << endl;
        SmallShell::fgPid = -1;
    }
}

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    auto fgPid = SmallShell::fgPid;

    cout << getpid() << endl;

    // Don't do anything if no fg process
    if (fgPid == -1) {
        return;
    }

    auto history = SmallShell::history;

    auto jobsList = SmallShell::jobsList;
    auto timeRes = getStartTime();
    auto cmd = history->getLastCmd();
    jobsList->addJob(cmd, fgPid, timeRes, true);

    auto killRes = kill(fgPid, SIGSTOP);

    if (killRes == -1) {
        logErrorSystemCall("kill");
    } else {
        cout << "smash: process " << fgPid << " was stopped" << endl;
        SmallShell::fgPid = -1;
    }

}
