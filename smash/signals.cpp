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

        // TODO: remove from jobs on kill
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

    auto jobsList = SmallShell::jobsList;
    auto time = getCurrentTime();

    auto lastJob = jobsList->getLastJob();

    lastJob->isStopped = true;
    lastJob->endTime = time;

    auto killRes = kill(fgPid, SIGSTOP);

    if (killRes == -1) {
        logErrorSystemCall("kill");
    } else {
        cout << "smash: process " << fgPid << " was stopped" << endl;
        SmallShell::fgPid = -1;
    }

}
