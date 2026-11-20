#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cmd.h"
#include "proc.h"

static const char *help = "help";
static const char *list = "list";
static const char *status = "status";
static const char *stat = "stat";
static const char *cmdline = "cmdline PID";
static const char *maps = "maps PID";
static const char *fd = "fd PID";
static const char *tasks = "tasks PID";
static const char *cpu = "cpuinfo";
static const char *mem = "meminfo";
static const char *uptime = "uptime";
static const char *quit = "quit";

#define INPUT_BUF_SIZE 24

int cmd_mode()
{
    int retval = 0;
    int in_menu = 1;
    char input_buf[INPUT_BUF_SIZE];
    char cmd[INPUT_BUF_SIZE];
    int arg = -1;

    fgets(input_buf, INPUT_BUF_SIZE, stdin); // Flush out the \n from menu entry

    while (in_menu)
    {
        if (fgets(input_buf, INPUT_BUF_SIZE, stdin) == NULL)
        {
            printf("Error reading STDIN\n");
        }
        else
        {
            // Strip possible trailing newline from fgets
            int buf_str_len = strnlen(input_buf, sizeof(input_buf));
            if (buf_str_len > 0 && input_buf[buf_str_len - 1] == '\n')
            {
                input_buf[buf_str_len - 1] = '\0';
            }

            // Parse input buffer
            int scan_match = sscanf(input_buf, "%s %d", cmd, &arg);
            printf("==========================================================================\n");

            // Only a string has been entered (or second item was not an int)
            if (scan_match == 1)
            {
                if (strncmp(cmd, help, INPUT_BUF_SIZE) == 0)
                {
                    // help_func
                    in_menu = 1;
                }
                else if (strncmp(cmd, list, INPUT_BUF_SIZE) == 0) // Exit CMD Mode
                {
                    in_menu = 0;
                }
                else if (strncmp(cmd, cpu, INPUT_BUF_SIZE) == 0)
                {
                    // Display cpuinfo
                    in_menu = 1;
                }
                else if (strncmp(cmd, mem, INPUT_BUF_SIZE) == 0)
                {
                    // Display meminfo
                    in_menu = 1;
                }
                else if (strncmp(cmd, uptime, INPUT_BUF_SIZE) == 0)
                {
                    // Display uptime info
                    in_menu = 1;
                }
                else if (strncmp(cmd, quit, INPUT_BUF_SIZE) == 0)
                {
                    // Quit command
                    in_menu = 0;
                    retval = -1;
                }
                else
                {
                    printf("Invalid Command\n");
                }
            }

            // A string and an int have been entered
            else if (scan_match == 2)
            {
                if (strncmp(cmd, status, INPUT_BUF_SIZE) == 0)
                {
                    // Display /proc/PID/status
                    print_pid_status(getuid(), arg);
                }
                else if (strncmp(cmd, stat, INPUT_BUF_SIZE) == 0)
                {
                    // stat display (see Appendix below)
                    print_pid_stat(getuid(), arg);
                }
                else if (strncmp(cmd, cmdline, INPUT_BUF_SIZE) == 0)
                {
                    // Display cmdline info
                    printf("%d\n", arg);
                }
                else if (strncmp(cmd, maps, INPUT_BUF_SIZE) == 0)
                {
                    // Display maps info
                    printf("%d\n", arg);
                }
                else if (strncmp(cmd, fd, INPUT_BUF_SIZE) == 0)
                {
                    // Display fd info
                    printf("%d\n", arg);
                }
                else if (strncmp(cmd, tasks, INPUT_BUF_SIZE) == 0)
                {
                    // Display tasks info
                    printf("%d\n", arg);
                }
                else
                {
                    printf("Invalid Command\n");
                }
            }

            // Invalid input
            else
            {
                printf("Invalid Command\n");
            }
        }

        printf("\n");
        // Clear buffers
        memset(input_buf, '\0', INPUT_BUF_SIZE);
        memset(cmd, '\0', INPUT_BUF_SIZE);

    }

    return retval;
}

//==================================================
// APPENDIX
//==================================================
/*
/proc/PID/stat fields:

#: Field        Meaning
1:  pid         Process ID
2:  comm        Parenthesized process name
3:  state       State code
4:  ppid        Parent PID
14: utime       Accumulated user-mode CPU ticks
15: stime       Accumulated kernel-mode CPU ticks
18: priority    Kernel-reported scheduling priority value
19: nice        Nice value
20: num_threads Thread count
22: starttime   Start time in ticks since boot
*/