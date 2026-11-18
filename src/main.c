/*-------------------------------
*
* CSCI 460: Operating Systems
* Montana State University
* Assignment 3: Process Monitor
*
* Griffin Short
*
*------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include "proc.h"
#include "utils.h"
#include "cmd.h"

static int prog_running = 1;

#define LIVE_MODE 1
#define CMD_MODE 2

static void signal_handler(int signal_num)
{
    if (signal_num == SIGINT)
    {
        prog_running = 0;
    }
}


int main(int argc, char *argv[])
{
    // Setup signal handler
    struct sigaction sig_act = { 0 };
    sig_act.sa_handler = &signal_handler;
    sigemptyset(&sig_act.sa_mask);
	sig_act.sa_flags = 0;

    if (sigaction(SIGINT, &sig_act, NULL) == -1)
	{
		fprintf(stderr, "Failed to setup sigaction for SIGINT.\n");
	}
    
    // Setup live terminal updates and menu entry polling
    struct pollfd poll_stdin = { .fd = STDIN_FILENO, .events = POLLIN };
    int prog_status = LIVE_MODE;

    // Program Loop (Ctrl+C terminated)
    while (prog_running)
    {
        if (prog_status == LIVE_MODE)
        {
            int ready = poll(&poll_stdin, 1, 2000); /* Timeout in milliseconds. */
    
            if (ready == 0) // 2 sec. timeout
            {
                printf("\033[2J\033[H"); /* Clear screen; move cursor to upper left. */
                print_user_procs();
                fflush(stdout);
            }
            else if (ready > 0) // An event occurred
            {
                if (check_revents(&poll_stdin) == POLLIN)
                {
                    prog_status = CMD_MODE;
                }
            }
            else if (ready == 1) // Signal Handler interrupted
            {
                // Nothing for now. Only Ctrl+C handled.
            }
        }

        // Let user enter commands.
        else if (prog_status == CMD_MODE)
        {
            if (cmd_mode() == -1)
            {
                prog_running = 0;
            }
            else
            {
                prog_status = LIVE_MODE;
            }
        }

    }

exit:
    // TODO: CLEANUP 
    // Exit
    return 0;

}
