#ifndef OS_HW1_WET_UTILS_H
#define OS_HW1_WET_UTILS_H

#include <cstdio>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <sys/types.h>
#include <sys/wait.h>

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif


using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

inline string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

inline string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

inline string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

inline int _parseCommandLine(const char *cmd_line, char **args) {
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

inline bool isBackgroundCommand(const char *cmd_line) {
    const string whitespace = " \t\n";
    const string str(cmd_line);
    return str[str.find_last_not_of(whitespace)] == '&';
}


inline bool isPipeCommand(const char *cmd_line) {
    const string str(cmd_line);
    if (str.find('|') == std::string::npos)
        return false;
    else
        return str[str.find('|')] == '|';
}

inline bool isRedirectionCommand(const char *cmd_line) {
    const string str(cmd_line);
    if (str.find('>') == std::string::npos)
        return false;
    else
        return str[str.find('>')] == '>';
}


inline void removeBackgroundSign(char *cmd_line) {
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

inline void logSysCallError(const string &sys_call_name) {
    string message = "smash error: " + sys_call_name + " failed";
    perror(message.c_str());
}

inline int toNumber(const std::string &s) {
    int i;
    try {
        i = std::stoi(s);
    } catch (std::invalid_argument &) {
        return -1;
    }

    return i;
}

inline int parseSignalArg(const std::string &s) {
    string signalStr;
    signalStr.reserve(s.size());
    for (size_t i = 1; i < s.size(); ++i) signalStr += s[i];
    return toNumber(signalStr);
}

inline void logError(const string &message) {
    cout << "smash error: " << message << endl;
}

inline time_t getCurrentTime() {
    time_t currTime;
    auto timeRes = time(&currTime);

    if (timeRes == -1) {
        logSysCallError("time");
    }

    return currTime;
}

#endif //OS_HW1_WET_UTILS_H
