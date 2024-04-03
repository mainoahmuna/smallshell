# Small Shell

## Description
Small Shell is a simple shell program that can execute commands, handle input/output redirection, and manage background processes.

## Usage
Compile program using C compiler:
gcc -std=c99 -D_XOPEN_SOURCE -g main.c -o smallsh

## Run the Program
./smallsh

## Features
Executes commands entered by the user.
Handles input/output redirection using < and > symbols.
Manages background processes with the & symbol.
Supports built-in commands:
cd: Change directory.
status: Print the exit status of the last executed command.
exit: Exit the shell.
Ignores SIGINT (Ctrl+C) when in foreground mode.
Toggles between foreground-only mode and normal mode with SIGTSTP (Ctrl+Z).

## Input Format
The shell accepts commands entered by the user. Each command can contain multiple arguments separated by spaces. The following special symbols are supported:

'<' : Input redirection. Redirects input from a file.
'>' : Output redirection. Redirects output to a file.
'&' : Background execution. Executes a command in the background.

## Built in Commands
### cd
change directory
### status
prints the exit status of the last executed command
### exit
exit the shell
