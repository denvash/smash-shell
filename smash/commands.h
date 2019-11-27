#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <utility>
#include <vector>
#include <iomanip>
#include "utils.h"
#include <unistd.h>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)
#define COMMAND_LENGTH (80)
#define MAX_JOBS (100)

using namespace std;

class Command {
public:
    string cmdLine;

    explicit Command(string cmdLine) : cmdLine(std::move(cmdLine)) {

    }

    virtual ~Command() = default;

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class JobsList {
public:
    struct JobEntry {
        pid_t pid;
        Command *cmd;
        int jobId;
        time_t startTime;
        time_t endTime;
        bool isStopped;

        JobEntry(pid_t pid, Command *cmd, int jobId,
                 time_t startTime,
                 time_t endTime = -1,
                 bool isStopped = false) : pid(pid),
                                           cmd(cmd),
                                           jobId(jobId),
                                           startTime(
                                                   startTime),
                                           endTime(),
                                           isStopped(
                                                   isStopped) {}

        void print() {
            std::cout << pid << ": " << cmd->cmdLine << endl;
        }
    };

    vector<JobEntry *> jobs;

    JobsList() : jobs() {
    };

    ~JobsList() = default;

    void addJob(Command *cmd, pid_t pid, time_t startTime, bool isStopped = false) {
        time_t currentTime;
        auto resTime = time(&currentTime);

        if (resTime == -1) {
            logErrorSystemCall("time");
        }

        jobs.push_back(new JobEntry(pid, cmd, (int) jobs.size() + 1, startTime, currentTime, isStopped));
    }

    void addJob(JobEntry *job) {
        job->jobId = (int) jobs.size() + 1;
        jobs.push_back(job);
    }

    void printJobsList() {
        for (auto jobEntry : jobs) {
            auto jobId = jobEntry->jobId;
            auto cmd_line = jobEntry->cmd->cmdLine;
            auto pid = jobEntry->pid;
            auto startTime = jobEntry->startTime;
            auto endTime = jobEntry->endTime;
            auto isStopped = jobEntry->isStopped;

            time_t currentTime;
            auto resTime = time(&currentTime);

            if (resTime == -1) {
                logErrorSystemCall("time");
            }

            auto time = difftime(isStopped ? endTime : currentTime, startTime);

            cout << "[" << jobId << "] " << cmd_line << " : " << pid << " " << time << " secs" <<
                 (isStopped ? " (stopped)" : "") << endl;
        }
    }

    void killAllJobs() {
        for (auto &job : jobs) {
            auto killRes = kill(job->pid, SIGKILL);
            if (killRes == -1) {
                logErrorSystemCall("kill");
            } else {
                job->print();
            }
        }
    }

    void removeFinishedJobs();

    JobEntry *getJobById(size_t jobId) {
        return jobId - 1 < jobs.size() ? jobs[jobId - 1] : nullptr;
    }

    JobEntry *getJobByPid(pid_t pid) {
        for (auto &job : jobs) {
            if (job->pid == pid) {
                return job;
            }
        }
        return nullptr;
    }

    void removeJobById(int jobId) {
        jobs.erase(jobs.begin() + jobId - 1);
    }

    JobEntry *getLastJob() {
        return jobs.empty() ? nullptr : jobs.back();
    }

    JobEntry *getLastStoppedJob(int *jobId);
};

class CommandsHistory {
private:
    int current_index;
    bool isOverlap;
    int time;
public:
    class CommandHistoryEntry {
        int timestamp;

    public:
        Command *cmd;

        CommandHistoryEntry() : timestamp(), cmd() {}

        explicit CommandHistoryEntry(Command *cmd, int timestamp) : timestamp(timestamp), cmd(cmd) {}

        void setTimestamp(int time) {
            timestamp = time;
        }

        bool isEqual(const string &cmd_line) {
            return cmd->cmdLine == cmd_line;
        }

        void print() {
            cout << right << setw(5) << timestamp << " " << cmd->cmdLine << endl;
        }
    };

    CommandsHistory() : current_index(0), isOverlap(false), time(0), history() {

    }

    ~CommandsHistory() = default;

    CommandHistoryEntry *history[HISTORY_MAX_RECORDS];

    void addRecord(Command *cmd) {
        increaseTime();
        auto lastIndex = (current_index == 0 ? HISTORY_MAX_RECORDS : current_index) - 1;

        auto last = history[lastIndex];
        if (last != nullptr && last->isEqual(cmd->cmdLine)) {
            last->setTimestamp(time);
        } else {
            history[current_index] = new CommandHistoryEntry(cmd, time);
            current_index++;
        }

        if (current_index == HISTORY_MAX_RECORDS) {
            current_index = 0;
            if (!isOverlap) {
                isOverlap = true;
            }
        }
    }

    void printHistory() {
        int printIndex = isOverlap ? current_index : 0;
        int startIndex = printIndex;
        int printedOnce = false;

        while (true) {
            if ((isOverlap && printedOnce && printIndex == startIndex) ||
                (!isOverlap && printIndex == HISTORY_MAX_RECORDS)) {
                break;
            }
            auto current = history[printIndex];
            printIndex++;

            if (isOverlap && printIndex == HISTORY_MAX_RECORDS) {
                printIndex = 0;
            }

            if (current == nullptr) {
                continue;
            }
            current->print();
            printedOnce = true;
        }
    }

    void increaseTime() {
        time++;
    }

    Command *getLastCmd() {
        auto lastIndex = (isOverlap && current_index == 0 ? HISTORY_MAX_RECORDS : current_index) - 1;
        return history[lastIndex]->cmd;
    }
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(string cmdLine) : Command(std::move(cmdLine)) {

    }

    ~BuiltInCommand() override = default;
};

class ExternalCommand : public Command {
public:
    explicit ExternalCommand(string cmdLine) : Command(std::move(cmdLine)) {};

    ~ExternalCommand() override = default;

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {}

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {}

    void execute() override;
    //void prepare() override;
    //void cleanup() override;
};

/* ================ Built In Commands ================ */

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(string cmdLine) : BuiltInCommand(std::move(cmdLine)) {

    }

    ~GetCurrDirCommand() override = default;

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    const string &path;


    explicit ChangeDirCommand(const char *cmd_line, const string &path) : BuiltInCommand(cmd_line), path(path) {

    }

    ~ChangeDirCommand() override = default;

    void execute() override;
};

class HistoryCommand : public BuiltInCommand {
    CommandsHistory *_history;
public:
    explicit HistoryCommand(string cmdLine, CommandsHistory *history) : BuiltInCommand(cmdLine),
                                                                        _history(history) {

    }

    ~HistoryCommand() override = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(string cmdLine) : BuiltInCommand(std::move(cmdLine)) {}

    ~ShowPidCommand() override = default;

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    bool isKill;
    JobsList *jobs;

public:
    QuitCommand(string cmdLine, bool isKill, JobsList *jobs) : BuiltInCommand(std::move(cmdLine)), isKill(isKill),
                                                               jobs(jobs) {}

    ~QuitCommand() override = default;

    void execute() override;
};

class JobsCommand : public BuiltInCommand {
    JobsList *_jobs;
public:
    JobsCommand(string cmdLine, JobsList *jobs) : BuiltInCommand(std::move(cmdLine)), _jobs(jobs) {
    }

    ~JobsCommand() override = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    int signal;
    pid_t pid;
public:
    KillCommand(string cmdLine, int signal, pid_t pid) : BuiltInCommand(std::move(cmdLine)), signal(signal), pid(pid) {}

    ~KillCommand() override = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList::JobEntry *job;
public:
    ForegroundCommand(string cmdLine, JobsList::JobEntry *jobEntry) : BuiltInCommand(std::move(cmdLine)),
                                                                      job(jobEntry) {}

    ~ForegroundCommand() override = default;

    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    BackgroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~BackgroundCommand() {}

    void execute() override;
};

class CopyCommand : public BuiltInCommand {
public:
    CopyCommand(const char *cmd_line);

    virtual ~CopyCommand() {}

    void execute() override;
};

/* ================ Shell ================ */

class SmallShell {
private:
    SmallShell() {
        last_pwd = "";
        history = new CommandsHistory();
        jobsList = new JobsList();
        fgProcess = nullptr;
    }


public:
    static string last_pwd;
    static CommandsHistory *history;
    static JobsList *jobsList;
    static JobsList::JobEntry *fgProcess;

    static Command *createCommand(const string &cmdLine);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() { // makes SmallShell singleton
        static SmallShell instance; // Guaranteed to be destroyed.
        return instance; // Instantiated on first use.
    }

    ~SmallShell() = default;

    static void executeCommand(const char *cmdBuffer);
};

#endif //SMASH_COMMAND_H_
