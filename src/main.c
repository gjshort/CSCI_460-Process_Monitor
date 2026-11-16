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
#include "proc.h"

static int prog_running = 1;

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
    

    // Program Loop (Ctrl+C terminated)
    while (prog_running)
    {
        printf("\033[2J\033[H"); /* Clear screen; move cursor to upper left. */
        print_user_procs();
        fflush(stdout);

        sleep(2); // Must replace with poll() later
    }

    // TODO: CLEANUP 
    // Exit

    return 0;
}
