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
        logSysCallError("kill");
    } else {
        cout << "smash: process " << fg->pid << " was killed" << endl;

        auto jobsList = SmallShell::jobsList;
        jobsList->removeJobByPid(fg->pid);
        delete SmallShell::fgProcess;
        SmallShell::fgProcess = nullptr;
    }
}

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    auto fg = SmallShell::fgProcess;

    // Don't do anything if no fg process
    if (fg->pid == -1) {
        return;
    }

    auto killRes = kill(fg->pid, SIGSTOP);

    if (killRes == -1) {
        logSysCallError("kill");
    } else {
        auto jobsList = SmallShell::jobsList;
        fg->endTime = getCurrentTime();
        fg->isStopped = true;

        jobsList->addJob(fg);

        cout << "smash: process " << fg->pid << " was stopped" << endl;
        SmallShell::fgProcess = nullptr;
    }

}
