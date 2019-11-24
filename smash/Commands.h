#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <iomanip>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)
#define COMMAND_LENGTH (80)

using namespace std;

class Command {
public:
    const char *cmd_line;

    explicit Command(const char *cmd_line) : cmd_line(cmd_line) {

    }

    virtual ~Command() = default;

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

/* ================ Built In Commands ================ */

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char *cmd_line) : Command(cmd_line) {

    }

    ~BuiltInCommand() override = default;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {

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

class CommandsHistory {
public:
    class CommandHistoryEntry {
        const string _cmd_line;
        int timestamp;

    public:
        CommandHistoryEntry() : _cmd_line(), timestamp() {}

        explicit CommandHistoryEntry(const string &cmd_line, int timestamp) : _cmd_line(string(cmd_line)),
                                                                              timestamp(timestamp) {}

        void setTimestamp(int time) {
            timestamp = time;
        }

        bool isEqual(const string &cmd_line) {
            return _cmd_line == cmd_line;
        }

        void print() {
            cout << right << setw(5) << timestamp << " " << _cmd_line << endl;
        }
    };

    int current_index;
    bool isOverlap;
    int time;

    CommandHistoryEntry *history[HISTORY_MAX_RECORDS];

    CommandsHistory() : current_index(0), isOverlap(false), time(0), history() {

    }

    ~CommandsHistory() = default;

    static void addRecord(const string &cmd_line, CommandsHistory *commandsHistory) {
        commandsHistory->increaseTime();
        auto current_index = commandsHistory->current_index;
        auto lastIndex = (current_index == 0 ? HISTORY_MAX_RECORDS : current_index) - 1;

        auto history = commandsHistory->history;
        auto last = history[lastIndex];
        if (last != nullptr && last->isEqual(cmd_line)) {
            last->setTimestamp(commandsHistory->time);
        } else {
            history[current_index] = new CommandHistoryEntry(cmd_line, commandsHistory->time);
            commandsHistory->current_index++;
        }

        if (current_index == HISTORY_MAX_RECORDS) {
            auto isOverlap = commandsHistory->isOverlap;
            commandsHistory->current_index = 0;
            if (!isOverlap) {
                commandsHistory->isOverlap = true;
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
};

class HistoryCommand : public BuiltInCommand {
    CommandsHistory *_history;
public:
    explicit HistoryCommand(const char *cmd_line, CommandsHistory *history) : BuiltInCommand(cmd_line),
                                                                              _history(history) {

    }

    ~HistoryCommand() override = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

    ~ShowPidCommand() override = default;

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
// TODO: Add your data members public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {}

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
        // TODO: Add your data members
    };
    // TODO: Add your data members
public:
    JobsList() = default;

    ~JobsList();

    void addJob(Command *cmd, bool isStopped = false);

    void printJobsList() {
        cout << "hello" << endl;
    }

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);
    // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
    JobsList *_jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), _jobs(jobs) {
    }

    ~JobsCommand() override = default;

    void execute() override {
        _jobs->printJobsList();
    }
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {}

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {}

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

/* ================ End Built In Commands ================ */



/* ================ External Commands ================ */

class ExternalCommand : public Command {
public:
    explicit ExternalCommand(const char *cmd_line) : Command(cmd_line) {};
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

/* ================ End External Commands ================ */

class SmallShell {
private:
    SmallShell();

public:
    static string last_pwd;
    static CommandsHistory *history;
    static JobsList *jobsList;

    static Command *createCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    static void executeCommand(const char *cmd_line);

    static void logError(const string &message);

    static void logDebug(const string &message);
    // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
