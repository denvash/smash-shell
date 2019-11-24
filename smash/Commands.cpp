#include <unistd.h>
#include <iostream>
#include <vector>
#include <sys/wait.h>
#include <ctime>
#include "Commands.h"

using namespace std;

string SmallShell::last_pwd;
CommandsHistory *SmallShell::history;
JobsList *SmallShell::jobsList;

void SmallShell::executeCommand(const char *cmdBuffer) {
    char cmdCopy[COMMAND_LENGTH];
    strcpy(cmdCopy, cmdBuffer);

    bool isBgCmd = isBackgroundCommand(cmdCopy);
    removeBackgroundSign(cmdCopy);

    auto tweakedCmdLine = string(cmdCopy);

    auto cmd = createCommand(tweakedCmdLine);
    if (cmd == nullptr) {
        return;
    }

    history->addRecord(tweakedCmdLine);

    if (isBgCmd) {
        auto pid = fork();

        if (pid == 0) {
            cmd->execute();
            exit(0);
        } else if (pid == -1) {
            systemCallError("fork");
        } else {
            time_t jobStartTime;
            auto timeRes = time(&jobStartTime);

            if (timeRes == -1) {
                systemCallError("time");
            }
            char fullCmd[COMMAND_LENGTH];
            strcpy(fullCmd, cmdBuffer);
            cmd->cmd_line = string(fullCmd);
            jobsList->addJob(cmd, pid, jobStartTime);
        }
    } else {
        cmd->execute();
        int wstatus;
        waitpid(0, &wstatus, WNOHANG);
    }

}

Command *SmallShell::createCommand(const string &cmdLine) {
    if (cmdLine.empty()) {
        return nullptr;
    }

    char *args_chars[COMMAND_MAX_ARGS];

    int args_size = _parseCommandLine(cmdLine.c_str(), args_chars);
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
            string arg = args[1];
            bool isRoot = arg == "-";

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
    }
    return new ExternalCommand(cmdLine);
}

void ChangeDirCommand::execute() {
    string back_up_last_pwd = string(SmallShell::last_pwd);

    if (SmallShell::last_pwd.empty()) {
        char cwd[COMMAND_LENGTH];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            SmallShell::last_pwd = string(cwd);
        } else {
            systemCallError("getcwd");
        }
    }

    int status = chdir(path.c_str());
    if (status != 0) {
        if (!SmallShell::last_pwd.empty()) {
            SmallShell::last_pwd = string(back_up_last_pwd);
        }
        systemCallError("chdir");
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

void ExternalCommand::execute() {
    char str_buf[COMMAND_LENGTH];
    strcpy(str_buf, cmd_line.c_str());
    char *args[] = {(char *) "/bin/bash", (char *) "-c", str_buf, nullptr};
    int pid = fork();
    if (pid == 0) {
        int res = execv(args[0], args);
        if (res == -1) {
            systemCallError("execv");
        }
        exit(0);
    } else if (pid == -1) {
        systemCallError("fork");
    }
    wait(nullptr);
}

void HistoryCommand::execute() {
    _history->printHistory();
}

void JobsCommand::execute() {
    _jobs->printJobsList();
}
