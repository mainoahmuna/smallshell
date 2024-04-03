/*******************************************************************
** Program: smallshell
** Author: Mainoah-Zander T Muna
** Date: 2/14/2024
** Description: A simple shell program that can execute commands, handle 
input/output redirection, and manage background processes.
*********************************************************************/
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include <fcntl.h>

//limits for input items running in the background and input and arguments
#define MAX_INPUT 2048
#define MAX_ARG 500
#define MAX_BACKGROUND_ITEMS 100

//global variables
int background_status = 1; //0 is forground only mode
int background_processes[MAX_BACKGROUND_ITEMS];
int exit_status = 0;

//struct to store values in the struct
struct Command_line{
    char *args[MAX_ARG];
    char *input_file;
    char *output_file;
    int background_cmd;
};

/*********************************************************************
** Function: get_input
** Description: Gets input from the user and parses it into a Command_line struct.
** Parameters: None.
** Pre-Conditions: None.
** Post-Conditions: Returns a Command_line struct containing the parsed input.
*********************************************************************/
struct Command_line get_input()
{
    //initialize command_line struct
    struct Command_line cl;

    //initialize args and struct items
    for (int i = 0; i < MAX_ARG; i++)
    {
        cl.args[i] = NULL;
    }
    cl.input_file = NULL;
    cl.output_file = NULL;
    cl.background_cmd = 0;

    //get user input
    char* user_input = malloc(MAX_INPUT * sizeof(char));
    printf(":");
    fflush(stdout);
    fgets(user_input, MAX_INPUT, stdin);

    //process user input
    int new_line = 0;
    size_t len = MAX_INPUT;
   
    //replace new line with null character
    for (int i = 0; i < len && !new_line; i++)
    {
        if (user_input[i] == '\n')
        {
            user_input[i] = '\0';
            new_line = 1;
        }
    }

    //check for blank lines and comments
    if (strcmp(user_input, "") == 0 || user_input[0] == '#')
    {
        return cl;
    }

    //insert process ID into user input
    insert_PID(user_input);
    
    //tokenize user input
    char *save_ptr;
    char *token = strtok_r(user_input, " ", &save_ptr);
    int i = 0;

    while (token != NULL)
    {   
        // Check for input redirection symbol "<"
        if (strcmp(token, "<") == 0)
        {
            token = strtok_r(NULL, " ", &save_ptr); // Skip "<"
            cl.input_file = strdup(token);          // Set input file

            if (cl.input_file == NULL)
            {
                printf("missing argument for input redirect\n");
            }
        }
        // Check for output redirection symbol ">"
        else if (strcmp(token, ">") == 0)
        {
            token = strtok_r(NULL, " ", &save_ptr); // Skip ">"
            cl.output_file = strdup(token);         // Set output file
            if (cl.output_file == NULL)
            {
                printf("missing argument for output redirect\n");
            }  
        }
        else
        {
            // Regular command argument
            cl.args[i] = strdup(token);
            i++;
        }
        token = strtok_r(NULL, " ", &save_ptr);
    }

    //for testing
    // printf("cl:\n");
    // for (int i = 0; cl.args[i] != NULL; i++)
    // {
    //     printf("cl[%d]: %s\n", i, cl.args[i]);
    // }
    
    free(user_input);
    return cl;
}

//Testing function
// void catch_SIGINT(int signo){
//     char* message = "Caught SIGINT, IGNORED\n";
//     write(STDOUT_FILENO, message, 38);
// }

/*********************************************************************
** Function: catch_SIGTSTP
** Description: Handles the SIGTSTP signal. Toggles between foreground-only mode and normal mode.
** Parameters: 
**     signo: The signal number.
** Pre-Conditions: None.
** Post-Conditions: Modifies the background_status variable accordingly.
*********************************************************************/
void catch_SIGTSTP(int signo){
    //toggle between foreground-only mode and normal mode based on background_status
    if (background_status == 1)
    {
        //change then inform user
        background_status = 0;
        write(1, "\nEntering foreground-only mode (& is now ignored)\n", 50);
    }else
    {
        //change and inform and exit foreground mode
        background_status = 1;
        write(1, "\n Exiting foreground only mode\n", 30);
    } 
}


/*********************************************************************
** Function: insert_PID
** Description: Inserts the process ID (PID) into a string by replacing occurrences of "$$" with the actual PID.
** Parameters: 
**     og_str: The original string where the PID will be inserted.
** Pre-Conditions: og_str must be a valid C string.
** Post-Conditions: Modifies og_str to replace occurrences of "$$" with the PID.
*********************************************************************/
void insert_PID(char *og_str){
    //get the current process ID(PID)
    pid_t pid = getpid();
    //determine length of PID string
    int pid_len = snprintf(NULL, 0, "%d", pid);
    //create a string to hold the PID
    char pid_str[pid_len + 1];

    //Convert PID to string
    sprintf(pid_str, "%d", pid);

    //get the length of original string
    int og_len = strlen(og_str);
    //create a new string to hold the modified string with PID
    char new_str[og_len * pid_len + 1];

    int i = 0;
    int j = 0;
    //iterate through the original string
    while (i < og_len)
    {
        //check for occurrences of "$$""
        if (og_str[i] == '$' && og_str[i + 1] == '$')
        {
            //replace $$ with PID
            strcpy(&new_str[j], pid_str);
            j += pid_len;
            i += 2;
        }else
        {
            //copy characters from the original string to the new string 
            new_str[j++] = og_str[i++];
        } 
    }

    //add null terminator to new string
    new_str[j] = '\0';
    //copy the modified string to the original string
    strcpy(og_str, new_str);
}

/*********************************************************************
** Function: execute_command
** Description: Executes the command provided in the Command_line struct, handles built-in commands such as cd and exit, and executes non-built-in commands using fork and execvp.
** Parameters: 
**     cl: Command_line struct containing the command and its arguments.
**     exit_status: The exit status of the previously executed command.
** Pre-Conditions: cl must be a valid Command_line struct.
** Post-Conditions: Executes the command and updates the exit_status if necessary.
*********************************************************************/
void execute_command(struct Command_line cl, int exit_status){
    //process command args for special cases background and input/output redirection
    for (int i = 0; cl.args[i] != NULL; i++)
    {
        if (strcmp(cl.args[i], "&") == 0)
        {
            //check if background mode mode is enabled
            if (background_status == 1)
            {
                printf("background mode is on\n");
                fflush(stdout);
                cl.background_cmd = 1;
            }else
            {
                //if background is off remove &
                cl.args[i] = NULL;
            }
        }
    }
    
    //check for built in commands and execute them
    for (int i = 0; cl.args[i] != NULL; i++)
    {
        if (strcmp(cl.args[0], "status") == 0)
        {
            //handle status
            status(exit_status);
            return;
        }else if (strcmp(cl.args[0], "cd") == 0)
        {
            //handle cd
            cd(cl);
            return;
        }else if (strcmp(cl.args[0], "exit") == 0)
        {
            //exit
            printf("exiting.....\n");
            exit(0);
        }else
        {
            //non builtin commands
            non_builtin(cl, &exit_status);
            return;
        }
    }   
}

/*********************************************************************
** Function: cd
** Description: Changes the current working directory based on the provided path.
** Parameters: 
**     cl: Command_line struct containing the directory path.
** Pre-Conditions: cl must be a valid Command_line struct.
** Post-Conditions: Changes the current working directory.
*********************************************************************/
void cd(struct Command_line cl){
    if (cl.args[1] == NULL)
    {
        //change to home directory if no path is provided
        //printf("changed to home dir\n");
        chdir(getenv("HOME"));
    }else
    {
        //change to specified direction
        //printf("changed dir to %s\n", cl.args[1]);
        if (chdir(cl.args[1]) != 0)
        {
            perror("chdir failed\n");
        }
    }
}

/*********************************************************************
** Function: status
** Description: Prints the exit status of the last executed command.
** Parameters: 
**     exit_status: The exit status of the last executed command.
** Pre-Conditions: None.
** Post-Conditions: Prints the exit status.
*********************************************************************/
void status(int exit_status){
    if (WIFEXITED(exit_status))
    {
        // If the process exited normally, print the exit value
        printf("exit value %d\n", WEXITSTATUS(exit_status));
    }else
    {
        // If the process was terminated by a signal, print the signal number
        printf("terminated by signal %d\n", exit_status);
    }
}

/*********************************************************************
** Function: non_builtin
** Description: Executes a non-built-in command using fork and execvp, and handles input/output redirection and background processes.
** Parameters: 
**     cl: Command_line struct containing the command and its arguments.
**     exit_status: Pointer to the exit status of the last executed command.
** Pre-Conditions: cl must be a valid Command_line struct.
** Post-Conditions: Executes the command and updates the exit_status if necessary.
*********************************************************************/
void non_builtin(struct Command_line cl, int* exit_status){
    int arg_count = 0;
    for (int i = 0; i < MAX_ARG; i++)
    {
        if (cl.args[i] != NULL)
        {
            arg_count++;
        }else
        {
            break;
        }
    }
    
    pid_t child = fork();

    switch (child)
    {
    case -1:
        printf("fork failed\n");
        exit(1);
        break;

    case 0:
        if (cl.background_cmd == 1 && background_status != 0)
        {
            if (cl.output_file == NULL)
            {
                //handle background command
                int fd_output = open("/dev/null", O_WRONLY);
                fcntl(fd_output, F_SETFD, FD_CLOEXEC);
                dup2(fd_output, 1);
            }
            if (cl.input_file == NULL)
            {
                int fd_input = open("/dev/null", O_WRONLY);
                fcntl(fd_input, F_SETFD, FD_CLOEXEC);
                dup2(fd_input, 1);
            } 
        }else
        {
            struct sigaction SIGINT_action = {{0}};
            SIGINT_action.sa_handler = SIG_DFL;
        }

        struct sigaction ignore_action = { { 0 } };
		ignore_action.sa_handler = SIG_IGN;

        //handle input redirection
        if (cl.input_file != NULL)
        {
            int fd_in = open(cl.input_file, O_RDONLY);
            if (fd_in == -1)
            {
                perror("Couldn't open input file\n");
                fflush(stdout);
                exit(EXIT_FAILURE);
            }else 
            {
                fcntl(fd_in, F_SETFD, FD_CLOEXEC);
                int in_result = dup2(fd_in, 0);
                if (in_result == -1)
                {
                    perror("input file error\n");
                    exit(EXIT_FAILURE);
                }   
            }
        }

        //handle output direction
        if (cl.output_file != NULL)
        {
            int fd_out = open(cl.output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out == -1)
            {
                perror("couldn't open output file\n");
                fflush(stdout);
                exit(EXIT_FAILURE);
            }else
            {
                fcntl(fd_out, F_SETFD, FD_CLOEXEC);
                int out_result = dup2(fd_out, 1);
                if (out_result == -1)
                {
                    perror("output file error\n");
                    fflush(stdout);
                    exit(EXIT_FAILURE);
                }   
            }
        }

        //execute command using execvp
        if (execvp(cl.args[0], cl.args))
        {
            perror("execution error\n");
            fflush(stdout);
            exit(EXIT_FAILURE);
            break;
        }
    default:
        //parent process
        if (cl.background_cmd == 1 && background_status != 0)
        {
            //if background mode is enabled print background pid
            printf("Background PID: %d\n", child);
            fflush(stdout);

            for (int i = 0; i < MAX_BACKGROUND_ITEMS; i++)
            {
                //store background pid
                if (background_processes[i] == 0)
                {
                    background_processes[i] = child;
                    break;
                } 
            }
            int child_status;
            child = waitpid(child, &child_status, WNOHANG);
        }else
        {
            //if not background wait for child to finish
            int child_status = 0;
            child = waitpid(child, &child_status, 0);

            //update exit status
            if (child > 0)
            {
                if (WIFSIGNALED(child_status))
                {
                    printf("\nchild process %d terminated by signal%d\n", child, child_status);
                    fflush(stdout);
                    *exit_status = WEXITSTATUS(child_status);
                }else
                {
                    *exit_status = WEXITSTATUS(child_status);
                } 
            }
        }
        break;
    }
    fflush(stdout);
}

/*********************************************************************
** Function: check_background_items
** Description: Checks for completed background processes and prints their status.
** Parameters: None.
** Pre-Conditions: None.
** Post-Conditions: Prints the status of completed background processes.
*********************************************************************/
void check_background_items(){

    for (int i = 0; i < MAX_BACKGROUND_ITEMS; i++)
    {
        if (background_processes[i] != 0)
        {
            pid_t childID;
            int child_status;
            childID = waitpid(-1, &child_status, WNOHANG);

            if (childID > 0)
            {
                if (WIFEXITED(child_status))
                {
                    // If the process exited normally, print exit value
                    printf("Background pid %d is done: exit value %d\n", childID, child_status);
                    fflush(stdout);
                }else
                {
                    // If the process was terminated by a signal, print signal number
                    printf("Background pid %d is done: terminated by signal %d\n", childID, child_status);
                    fflush(stdout);
                }
            }   
        } 
    }
    return;
}


int main(){
    int background_status = 1;
    int exit_status = 0;
    struct Command_line cl;

    struct sigaction SIGINT_action = {0};
    struct sigaction SIGTSTP_action = {0};

    //ignore ctrl C
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    //ctrl Z action
    SIGTSTP_action.sa_handler = catch_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Main shell loop
    while(1){
        // Get user input
        cl = get_input();

        // Execute command
        execute_command(cl, &exit_status);

        // Check for completed background processes
        check_background_items();
    }

    return 0;

}