# Smash Shell

A Unix shell is a command-line interpreter or shell that provides a command-line user
interface for Unix-like operating systems [Wikipedia].

There are few well-known shells on Unix systems, one of the most used is Bash. Bash is a
command processor that typically runs in a text window. Users can interact with the Bash
shell by typing commands that cause some actions.

In this assignment, you are going to implement a “smash” (small shell) which will behave
like a real Linux shell but it will support only a limited subset of Linux shell commands.

- [Theoretical questions & answers](DHW1_320616105_314483686.pdf) (Hebrew)
- [Instructions](Instruction.pdf) (English)

## Installation

- With Makefile

```bash
cd smash
make
./smash  
```

- As CMake project - target `CmakeLists.txt` with your IDEA.

Simple running example:

```bash
smash> sleep 100& 
smash> sleep 200&
smash> jobs
[1] sleep 100& : 6115 3 secs
[2] sleep 200& : 6118 1 secs
smash> fg 1
6115: sleep 100&
^Zsmash: got ctrl-Z
smash: process 6115 was stopped
smash> jobs
[1] sleep 100& : 6115 10 secs (stopped)
[2] sleep 200& : 6118 9 secs
smash> bg
6115: sleep 100&
smash> jobs
[1] sleep 100& : 6115 16 secs
[2] sleep 200& : 6118 14 secs
smash> kill -9 1
signal number 9 was sent to pid 6115
smash> jobs
[2] sleep 200& : 6118 25 secs
smash> fg
6118: sleep 200&
^Zsmash: got ctrl-Z
smash: process 6118 was stopped
smash> sleep 100&
smash> sleep 200&
smash> jobs
[2] sleep 200& : 6118 28 secs (stopped)
[3] sleep 100& : 7619 11 secs
[4] sleep 200& : 7631 8 secs
smash> quit kill
smash: sending SIGKILL signal to 3 jobs:
6118: sleep 200&
7619: sleep 100&
7631: sleep 200&
```