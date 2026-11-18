#include <stdio.h>
#include <string.h>
#include "cmd.h"

static const char *help = "help";
static const char *list = "list";
static const char *status = "status PID";
static const char *stat = "stat PID";
static const char *cmdline = "cmdline PID";
static const char *maps = "maps PID";
static const char *fd = "fd PID";
static const char *tasks = "tasks PID";
static const char *cpu = "cpuinfo";
static const char *mem = "meminfo";
static const char *uptime = "uptime";
static const char *quit = "quit";

#define INPUT_BUF_SIZE 24

void cmd_mode()
{
    
    int in_menu = 1;
    char input_buf[INPUT_BUF_SIZE];

    while (in_menu)
    {
        if (fgets(input_buf, INPUT_BUF_SIZE, stdin) == NULL)
        {
            perror("Error reading STDIN\n");
        }
        else
        {
            // Strip possible trailing newline from fgets
            int buf_str_len = strnlen(input_buf, sizeof(input_buf));
            if (buf_str_len > 0 && input_buf[buf_str_len - 1] == '\n')
            {
                input_buf[buf_str_len - 1] = '\0';
            }

            if (strncmp(input_buf, help, INPUT_BUF_SIZE) == 0)
            {
                // help_func
                in_menu = 1;
            }
            else if (strncmp(input_buf, list, INPUT_BUF_SIZE) == 0) // Exit CMD Mode
            {
                in_menu = 0;
            }
            else if (strncmp(input_buf, status, INPUT_BUF_SIZE) == 0) // Exit CMD Mode
            {
                // Display /proc/PID/status
                in_menu = 1;
            }
            else if (strncmp(input_buf, stat, INPUT_BUF_SIZE) == 0) // Exit CMD Mode
            {
                // stat display
                in_menu = 1;
            }
            else if (strncmp(input_buf, cmdline, INPUT_BUF_SIZE) == 0) // Exit CMD Mode
            {
                // Display cmdline info
                in_menu = 1;
            }
            else
            {
                printf("Invalid Command\n");
            }
        }


    }

    return;
}