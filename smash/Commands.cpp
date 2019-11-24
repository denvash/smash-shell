#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

const std::string WHITESPACE = " \n\r\t\f\v";

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundCommand(const char *cmd_line) {
    const string whitespace = " \t\n";
    const string str(cmd_line);
    return str[str.find_last_not_of(whitespace)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string whitespace = " \t\n";
    const string str(cmd_line);
    // find last character other than spaces
    size_t idx = str.find_last_not_of(whitespace);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(whitespace, idx - 1) + 1] = 0;
}

void systemCallError(const string &sys_call_name) {
    string message = "smash error: " + sys_call_name + " failed";
    perror(message.c_str());
}

string SmallShell::last_pwd;
CommandsHistory *SmallShell::history;
JobsList *SmallShell::jobsList;

SmallShell::SmallShell() {
    SmallShell::history = new CommandsHistory();
    SmallShell::jobsList = new JobsList();
}

SmallShell::~SmallShell() {
    delete SmallShell::history;
//    delete SmallShell::jobsList;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // Must fork smash process for some commands (e.g., external commands....)
    bool isBgCmd = _isBackgroundCommand(cmd_line);
    _removeBackgroundSign((char *) cmd_line);

    CommandsHistory::addRecord(string(cmd_line), SmallShell::history);
    auto cmd = createCommand(cmd_line);

    if (isBgCmd) {
        auto pid = fork();
        if (pid == 0) {
            cmd->execute();
        } else if (pid == -1) {
            systemCallError("fork");
        }
    } else {
        cmd->execute();
        wait(nullptr);
    }

}

void SmallShell::logError(const string &message) {
    cout << "smash error: " << message << endl;
}

void SmallShell::logDebug(const string &message) {
    cout << "smash debug: " << message << endl;
}

Command *SmallShell::createCommand(const char *cmd_line) {
    string _cmdLine = string(cmd_line);

    if (_cmdLine.empty()) {
        return nullptr;
    }

    char *args_chars[COMMAND_MAX_ARGS];
    int args_size = _parseCommandLine(cmd_line, args_chars);
    string args[COMMAND_MAX_ARGS];

    for (int i = 0; i < args_size; i++) {
        args[i] = string(args_chars[i]);
        free(args_chars[i]);
    }

    string cmd = args[0];

    if (cmd == "pwd") {
        return new GetCurrDirCommand(cmd_line);

    } else if (cmd == "cd") {
        if (args_size > 2) {
            logError("cd: too many arguments");
        } else {
            string arg = args[1];
            bool isRoot = arg == "-";

            if (isRoot && last_pwd.empty()) {
                logError("cd: OLDPWD not set");
            } else {
                return new ChangeDirCommand(cmd_line, isRoot ? last_pwd : arg);
            }
        }
    } else if (cmd == "history") {
        return new HistoryCommand(cmd_line, SmallShell::history);
    } else if (cmd == "jobs") {
        return new JobsCommand(cmd_line, SmallShell::jobsList);
    } else if (cmd == "showpid") {
        return new ShowPidCommand(cmd_line);
    }
    return new ExternalCommand(cmd_line);
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
    char *args[] = {(char *) "/bin/bash", (char *) "-c", (char *) cmd_line, nullptr};
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
