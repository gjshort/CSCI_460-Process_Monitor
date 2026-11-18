#ifndef UTILS_H
#define UTILS_H

#include <poll.h>

int check_revents(struct pollfd *poll_stdin);

#endif