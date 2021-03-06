#include <unistd.h>
#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctime>
#include <fcntl.h>
#include "commands.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

string SmallShell::last_pwd;
CommandsHistory *SmallShell::history;
JobsList *SmallShell::jobsList;
JobEntry *SmallShell::fgProcess;

void setFg(Command *cmd, pid_t pid) {
    delete SmallShell::fgProcess;
    SmallShell::fgProcess = new JobEntry(pid, cmd, -1, getCurrentTime());
}

bool isFgCmd(Command *cmd) {
    return dynamic_cast<const ForegroundCommand *>(cmd) != nullptr;
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
            logSysCallError("fork");
        } else {
            auto jobStartTime = getCurrentTime();
            cmd->cmdLine = string(cmdBuffer);
            jobsList->addJob(cmd, pid, jobStartTime);
            jobsList->removeFinishedJobs();
        }
    } else {
        cmd->execute();

        if (!isFgCmd(cmd)) {
            delete SmallShell::fgProcess;
            auto time = getCurrentTime();
            SmallShell::fgProcess = new JobEntry(-1, cmd, -1, time);
        }

        // Jobs list cleanup (removing finished jobs) should be done after each executed command
        // https://piazza.com/class/k1yxdx0sx3926r?cid=170
        jobsList->removeFinishedJobs();
    }
}

Command *SmallShell::createCommand(const string &cmdLine) {
    if (cmdLine.empty()) {
        return nullptr;
    }

    //Check if pipeline or redirection command
    if (isPipeCommand(cmdLine.c_str()))
        return new PipeCommand(cmdLine);

    if (isRedirectionCommand(cmdLine.c_str()))
        return new RedirectionCommand(cmdLine);

    //Regular Command

    char *args_chars[COMMAND_MAX_ARGS];

    auto args_size = _parseCommandLine(cmdLine.c_str(), args_chars);

    string args[COMMAND_MAX_ARGS];

    for (int i = 0; i < args_size; i++) {
        args[i] = string(args_chars[i]);
        free(args_chars[i]);
    }

    string cmd = args[0];

    // Jobs list cleanup (removing finished jobs) should be done
    // before each command that is related to jobs list (jobs,fg,bg,kill,quit kill).
    // https://piazza.com/class/k1yxdx0sx3926r?cid=170

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
        jobsList->removeFinishedJobs();
        return new JobsCommand(cmdLine, jobsList);
    } else if (cmd == "showpid") {
        return new ShowPidCommand(cmdLine);
    } else if (cmd == "kill") {
        auto signalInput = args[1];
        auto signalNumber = parseSignalArg(signalInput);
        auto jobId = toNumber(args[2]);

        if (args_size > 3 || signalInput[0] != '-' || jobId == -1) {
            logError("kill: invalid arguments");
            return nullptr;
        }

        auto jobEntry = jobsList->getJobById(jobId);

        if (jobEntry == nullptr) {
            logError("kill: job-id " + to_string(jobId) + " does not exists");
            return nullptr;
        }

        jobsList->removeFinishedJobs();
        return new KillCommand(cmdLine, signalNumber, jobEntry->pid);
    } else if (cmd == "quit") {
        bool isKill = args_size > 1 && args[1] == "kill";
        if (isKill) {
            jobsList->removeFinishedJobs();
        }
        return new QuitCommand(cmdLine, isKill, jobsList);
    } else if (cmd == "fg") {
        jobsList->removeFinishedJobs();
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
        return new ForegroundCommand(cmdLine, jobEntry);
    } else if (cmd == "bg") {
        if (args_size == 1) {
            auto lastEntry = jobsList->getLastStoppedJob();

            if (lastEntry == nullptr) {
                logError("bg: there is no stopped jobs to resume");
                return nullptr;
            }

            jobsList->removeFinishedJobs();
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

        jobsList->removeFinishedJobs();
        return new BackgroundCommand(cmdLine, jobEntry);
    } else if (cmd == "cp") {
        auto pathSource = string(args[1]);
        auto pathTarget = string(args[2]);

        return new CopyCommand(cmdLine, pathSource, pathTarget);
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
            logSysCallError("execv");
        }
        exit(0);

    } else if (pid == -1) {
        logSysCallError("fork");
    } else {
        setFg(this, pid);
        int wstatus;
        waitpid(pid, &wstatus, WUNTRACED);
    }
}

void ForegroundCommand::execute() {
    job->print();

    auto killRes = kill(job->pid, SIGCONT);
    if (killRes == -1) {
        logSysCallError("kill");
    } else {
        // Remove the job from job list after bringing to fg
        auto jobList = SmallShell::jobsList;
        jobList->removeJobByPid(job->pid);

        delete SmallShell::fgProcess;
        SmallShell::fgProcess = job;

        int wstatus;
        waitpid(job->pid, &wstatus, WUNTRACED);
    }
}

void BackgroundCommand::execute() {
    job->print();

    auto killRes = kill(job->pid, SIGCONT);

    if (killRes == -1) {
        logSysCallError("kill");
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
            logSysCallError("getcwd");
        }
    }

    int status = chdir(path.c_str());
    if (status != 0) {
        if (!SmallShell::last_pwd.empty()) {
            SmallShell::last_pwd = string(back_up_last_pwd);
        }
        logSysCallError("chdir");
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
    jobs->removeFinishedJobs();
    jobs->printJobsList();
}

void KillCommand::execute() {
    cout << "signal number " << signal << " was sent to pid " << pid << endl;
    auto killRes = kill(pid, signal);

    if (killRes == -1) {
        logSysCallError("kill");
    } else {
        auto jobList = SmallShell::jobsList;
        auto job = jobList->getJobByPid(pid);
        if (job != nullptr) {
            jobList->removeJobById(job->jobId);
        }
    }
}

void QuitCommand::execute() {
    if (isKill) {
        jobs->killAllJobs();
    }
    exit(0);
}

void PipeCommand::execute() {
    int pipeSignIndex = cmdLine.find('|');
    bool isPipeStdErr = cmdLine[pipeSignIndex + 1] && cmdLine[pipeSignIndex + 1] == '&';
    auto cmdSource = SmallShell::createCommand(cmdLine.substr(0, pipeSignIndex));
    auto cmdTarget = SmallShell::createCommand(cmdLine.substr(pipeSignIndex + (int) isPipeStdErr + 1));

    int pipeLine[2];
    pipe(pipeLine);

    auto pid = fork();

    if (pid == -1)//fork fail
        logSysCallError("fork");
    else if (!pid) {//son proc
        setpgrp();
        if (close(pipeLine[1]) == -1)
            logSysCallError("close");
        auto newStdIn = dup(0);
        if (newStdIn == -1)
            logSysCallError("dup");
        if (dup2(pipeLine[0], 0) == -1)
            logSysCallError("dup2");

        cmdTarget->execute();

        if (close(pipeLine[0]) == -1 || close(newStdIn) == -1)
            logSysCallError("close");
        exit(0);
    } else {//father proc
        if (close(pipeLine[0]) == -1)
            logSysCallError("close");
        if (isPipeStdErr) {
            auto newStdErr = dup(2);
            if (newStdErr == -1)
                logSysCallError("dup");
            if (dup2(pipeLine[1], 2) == -1)
                logSysCallError("dup2");

            if (close(pipeLine[1]) == -1)
                logSysCallError("close");

            cmdSource->execute();

            if (dup2(newStdErr, 2) == -1)
                logSysCallError("dup2");
            if (close(newStdErr) == -1)
                logSysCallError("close");
        } else {
            auto newStdOut = dup(1);
            if (newStdOut == -1)
                logSysCallError("dup");
            if (dup2(pipeLine[1], 1) == -1)
                logSysCallError("dup2");
            if (close(pipeLine[1]) == -1)
                logSysCallError("close");

            cmdSource->execute();

            if (dup2(newStdOut, 1) == -1)
                logSysCallError("dup2");
            if (close(newStdOut) == -1)
                logSysCallError("close");
        }
        int wstatus;
        waitpid(pid, &wstatus, WUNTRACED);
    }
}

void RedirectionCommand::execute() {
    int redirectionSignIndex = cmdLine.find('>');

    bool isAppend = cmdLine[redirectionSignIndex + 1] && cmdLine[redirectionSignIndex + 1] == '>';
    bool justStdOut = cmdLine[redirectionSignIndex - 1] && cmdLine[redirectionSignIndex - 1] != '&';

    auto cmdSourceCopy(cmdLine.substr(0, redirectionSignIndex - 1));

    bool justBgAndOpen = isBackgroundCommand(cmdSourceCopy.c_str());

    removeBackgroundSign((char *) cmdSourceCopy.c_str());

    auto tweakedCmdLine = string(cmdSourceCopy);

    auto cmd = SmallShell::createCommand(tweakedCmdLine);

    char *args_chars[COMMAND_MAX_ARGS];
    int args_size = _parseCommandLine((char *) cmdLine.substr(redirectionSignIndex + (int) isAppend + 1)
            .c_str(), args_chars);
    if (args_size > 1)
        perror("too many arguments for redirect");
    else if (args_size == 0)
        perror("syntax error near unexpected token `newline'");
    else {
        int fdTarget;
        if (isAppend)
            fdTarget = open(args_chars[0], O_WRONLY | O_CREAT | O_APPEND, 0644);
        else
            fdTarget = open(args_chars[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);


        if (fdTarget == -1)
            logSysCallError("open");
        else {

            if (justBgAndOpen) {
                if (close(fdTarget) == -1)
                    logSysCallError("close");

                auto pid = fork();

                if (pid == 0) {
                    setpgrp();
                    cmd->execute();
                    exit(0);
                } else if (pid == -1) {
                    logSysCallError("fork");
                } else {
                    auto jobStartTime = getCurrentTime();
                    cmd->cmdLine = string(tweakedCmdLine);
                    SmallShell::jobsList->addJob(cmd, pid, jobStartTime);
//                    shell removes finished jobs when going back in the func stack
                }
            } else {
                auto newStdout = dup(1);
                auto newStdErr = dup(2);

                if (newStdout == -1 || newStdErr == -1)
                    logSysCallError("dup");
                if (dup2(fdTarget, 1) == -1)
                    logSysCallError("dup2");

                if (!justStdOut) {
                    if (dup2(fdTarget, 2) == -1)
                        logSysCallError("dup2");
                }

                if (close(fdTarget) == -1)
                    logSysCallError("close");

                cmd->execute();

                if (!justStdOut) {
                    if (dup2(newStdErr, 2) == -1)
                        logSysCallError("dup2");
                }

                if (dup2(newStdout, 1) == -1)
                    logSysCallError("dup2");
                if (close(newStdout) == -1 || close(newStdErr) == -1)
                    logSysCallError("close");

            }

        }
    }
}

void CopyCommand::execute() {

    auto fdSource = open(source.c_str(), O_RDONLY);
    auto fdTarget = open(target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fdSource == -1 || fdTarget == -1) {
        logSysCallError("open");
    } else {
        char buf[1024];

        while (true) {
            auto readCount = read(fdSource, &buf, 1024);

            if (readCount == -1) {
                logSysCallError("read");
            } else if (readCount == 0) {
                break;
            }

            auto writeRes = write(fdTarget, &buf, readCount);

            if (writeRes == -1) {
                logSysCallError("read");
            }


        }

        auto closeSource = close(fdSource);
        auto closeTarget = close(fdTarget);

        if (closeSource == -1 || closeTarget == -1) {
            logSysCallError("open");
        } else {
            cout << "smash: " << source << " was copied to " << target << endl;
        }
    }
}
