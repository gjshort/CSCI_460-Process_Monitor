#include <poll.h>
#include <stdio.h>
#include "utils.h"


int check_revents(struct pollfd *poll_stdin)
{
    if (poll_stdin->revents & POLLIN) // Data to be read from terminal
    {
        return POLLIN;
    }
    else if (poll_stdin->revents & POLLHUP)
    {
        perror("Poll: stdin held up\n");
        return -1;
    }
    else if (poll_stdin->revents & POLLERR)
    {
        perror("Poll: error condition\n");
        return -1;
    }
    else if (poll_stdin->revents & POLLNVAL)
    {
        perror("Poll: stdin not open\n");
        return -1;
    }
    
    return -1;
}