#include <unistd.h>
#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctime>
#include "commands.h"


using namespace std;

string SmallShell::last_pwd;
CommandsHistory *SmallShell::history;
JobsList *SmallShell::jobsList;
JobsList::JobEntry *SmallShell::fgProcess;

void setFg(Command *cmd, pid_t pid) {
    delete SmallShell::fgProcess;
    SmallShell::fgProcess = new JobsList::JobEntry(pid, cmd, -1, getCurrentTime());
}


void SmallShell::executeCommand(const char *cmdBuffer) {
    auto cmdCopy = string(cmdBuffer);

    bool isBgCmd = isBackgroundCommand(cmdCopy.c_str());
    removeBackgroundSign((char *) cmdCopy.c_str());

    auto tweakedCmdLine = string(cmdCopy);

    auto cmd = createCommand(tweakedCmdLine);
    if (cmd == nullptr) {
        return;
    }

    history->addRecord(cmd);

    if (isBgCmd) {
        auto pid = fork();

        if (pid == 0) {
            setpgrp();
            cmd->execute();
            exit(0);
        } else if (pid == -1) {
            logErrorSystemCall("fork");
        } else {
            auto jobStartTime = getCurrentTime();
            cmd->cmdLine = string(cmdBuffer);
            jobsList->addJob(cmd, pid, jobStartTime);
        }
    } else {
        cmd->execute();
        delete SmallShell::fgProcess;
        auto time = getCurrentTime();
        SmallShell::fgProcess = new JobsList::JobEntry(-1, cmd, -1, time);
    }
}

Command *SmallShell::createCommand(const string &cmdLine) {
    if (cmdLine.empty()) {
        return nullptr;
    }

    char *args_chars[COMMAND_MAX_ARGS];

    auto args_size = _parseCommandLine(cmdLine.c_str(), args_chars);

    string args[COMMAND_MAX_ARGS];

    for (int i = 0; i < args_size; i++) {
        args[i] = string(args_chars[i]);
        free(args_chars[i]);
    }

    string cmd = args[0];

    if (cmd == "pwd") {
        return new GetCurrDirCommand(cmdLine);

    } else if (cmd == "cd") {
        if (args_size > 2) {
            logError("cd: too many arguments");
        } else {
            auto arg = args[1];
            auto isRoot = arg == "-";

            if (isRoot && last_pwd.empty()) {
                logError("cd: OLDPWD not set");
            } else {
                return new ChangeDirCommand(cmdLine.c_str(), isRoot ? last_pwd : arg);
            }
        }
    } else if (cmd == "history") {
        return new HistoryCommand(cmdLine, history);
    } else if (cmd == "jobs") {
        return new JobsCommand(cmdLine, jobsList);
    } else if (cmd == "showpid") {
        return new ShowPidCommand(cmdLine);
    } else if (cmd == "kill") {
        auto signalInput = args[1];
        auto signalNumber = parseSignalArg(signalInput);
        auto jobId = toNumber(args[2]);

        if (args_size > 3 || signalInput[0] != '-' || isSignal(signalNumber) || jobId == -1) {
            logError("kill: invalid arguments");
            return nullptr;
        }

        auto jobEntry = jobsList->getJobById(jobId);

        if (jobEntry == nullptr) {
            logError("kill: job-id " + to_string(jobId) + " does not exists");
            return nullptr;
        }

        return new KillCommand(cmdLine, signalNumber, jobEntry->pid);
    } else if (cmd == "quit") {
        bool isKill = args_size > 1 && args[1] == "kill";
        return new QuitCommand(cmdLine, isKill, jobsList);
    } else if (cmd == "fg") {
        if (args_size == 1) {
            auto lastEntry = jobsList->getLastJob();

            if (lastEntry == nullptr) {
                logError("fg: jobs list is empty");
                return nullptr;
            }

            return new ForegroundCommand(cmdLine, lastEntry);
        }

        auto jobId = toNumber(args[1]);

        if (args_size > 2 || jobId == -1) {
            logError("fg: invalid arguments");
            return nullptr;
        }

        auto jobEntry = jobsList->getJobById(jobId);

        if (jobEntry == nullptr) {
            logError("fg: job-id " + to_string(jobId) + " does not exists");
            return nullptr;
        }

        return new BackgroundCommand(cmdLine, jobEntry);
    } else if (cmd == "bg") {
        if (args_size == 1) {
            auto lastEntry = jobsList->getLastStoppedJob();

            if (lastEntry == nullptr) {
                logError("bg: there is no stopped jobs to resume");
                return nullptr;
            }

            return new BackgroundCommand(cmdLine, lastEntry);
        }

        auto jobId = toNumber(args[1]);

        if (args_size > 2 || jobId == -1) {
            logError("bg: invalid arguments");
            return nullptr;
        }

        auto jobEntry = jobsList->getJobById(jobId);

        if (jobEntry == nullptr) {
            logError("bg: job-id " + to_string(jobId) + " does not exists");
            return nullptr;
        } else if (!jobEntry->isStopped) {
            logError("bg: job-id " + to_string(jobId) + " is already running in the background");
            return nullptr;
        }
        return new BackgroundCommand(cmdLine, jobEntry);
    } else {
        return new ExternalCommand(cmdLine);
    }
    return nullptr;
}

void ExternalCommand::execute() {
    auto cmdCopy = string(cmdLine);

    char *args[] = {(char *) "/bin/bash", (char *) "-c", (char *) cmdCopy.c_str(), nullptr};
    pid_t pid = fork();

    if (pid == 0) {
        setpgrp();
        int res = execv(args[0], args);
        if (res == -1) {
            logErrorSystemCall("execv");
        }
        exit(0);

    } else if (pid == -1) {
        logErrorSystemCall("fork");
    } else {
        setFg(this, pid);
        int wstatus;
        waitpid(-1, &wstatus, WUNTRACED);
    }
}

void ForegroundCommand::execute() {
    job->print();

    auto killRes = kill(job->pid, SIGCONT);
    if (killRes == -1) {
        logErrorSystemCall("kill");
    } else {
        // Remove the job from job list after bringing to fg
        auto jobList = SmallShell::jobsList;
        jobList->removeJobById(job->jobId);

        delete SmallShell::fgProcess;

        SmallShell::fgProcess = job;
        int wstatus;
        waitpid(-1, &wstatus, WUNTRACED);
    }
}

void BackgroundCommand::execute() {
    job->print();

    auto killRes = kill(job->pid, SIGCONT);

    if (killRes == -1) {
        logErrorSystemCall("kill");
    } else {
        job->isStopped = false;
    }
}

void ChangeDirCommand::execute() {
    string back_up_last_pwd = string(SmallShell::last_pwd);

    if (SmallShell::last_pwd.empty()) {
        char cwd[COMMAND_LENGTH];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            SmallShell::last_pwd = string(cwd);
        } else {
            logErrorSystemCall("getcwd");
        }
    }

    int status = chdir(path.c_str());
    if (status != 0) {
        if (!SmallShell::last_pwd.empty()) {
            SmallShell::last_pwd = string(back_up_last_pwd);
        }
        logErrorSystemCall("chdir");
    }

}

void GetCurrDirCommand::execute() {
    char cwd[COMMAND_LENGTH];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        cout << cwd << endl;
    }
}

void ShowPidCommand::execute() {
    pid_t id = getpid();
    cout << "smash pid is " << id << endl;
}

void HistoryCommand::execute() {
    _history->printHistory();
}

void JobsCommand::execute() {
    _jobs->printJobsList();
}

void KillCommand::execute() {
    cout << "signal number " << signal << " was sent to pid " << pid << endl;
    auto killRes = kill(pid, signal);

    if (killRes == -1) {
        logErrorSystemCall("kill");
    } else {
        auto jobList = SmallShell::jobsList;
        auto job = jobList->getJobByPid(pid);
        jobList->removeJobById(job->jobId);
    }
}

void QuitCommand::execute() {
    if (isKill) {
        jobs->killAllJobs();
    }
    exit(0);
}
