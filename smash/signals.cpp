#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include "commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    auto fg = SmallShell::fgProcess;

    // Don't do anything if no fg process
    if (fg->pid == -1) {
        return;
    }

    auto killRes = kill(fg->pid, SIGKILL);

    if (killRes == -1) {
        logErrorSystemCall("kill");
    } else {
        cout << "smash: process " << fg->pid << " was killed" << endl;

        // TODO: remove from jobs on kill
        SmallShell::fgProcess->resetPid();
    }
}

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    auto fg = SmallShell::fgProcess;

    // Don't do anything if no fg process
    if (fg->pid == -1) {
        return;
    }

    auto jobsList = SmallShell::jobsList;
    auto time = getCurrentTime();

    auto lastJob = jobsList->getLastJob();

    lastJob->isStopped = true;
    lastJob->endTime = time;

    auto killRes = kill(fg->pid, SIGSTOP);

    if (killRes == -1) {
        logErrorSystemCall("kill");
    } else {
        cout << "smash: process " << fg->pid << " was stopped" << endl;
        fg->pid = -1;
    }

}
