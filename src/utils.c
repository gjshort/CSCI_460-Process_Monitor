#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "utils.h"

static int get_proc_status(uid_t user_id, long pid)
{
    // Create file path to /proc/pid/status
    char path[128];
    int n = snprintf(path, sizeof(path), "/proc/%ld/status", pid);
    if (n < 0 || (size_t)n >= sizeof(path))
    {
        perror("Invalid /proc/<pid>/status path");
        return -1;
    }
    
    // Open proc's status file
    FILE *proc_file = fopen(path, "r");
    if (proc_file == NULL)
    {
        perror(path);
        return -1;
    }

    // Parse fields
    char buffer[4096];
    char field[36];
    char value[36];

    char name[24];
    char pid_str[24];
    char ppid[24];
    char state[24];
    char mem[24];
    char threads[24];

    while (fgets(buffer, sizeof(buffer), proc_file) != NULL)
    {
        int scan_ret = sscanf(buffer, "%35[^:]: %35[^, \n]", field, value);
        if (scan_ret == 2)
        {
            if (strncmp(field, "Name", 4) == 0)
            {
                strncpy(name, value, sizeof(name));
            }
            else if (strncmp(field, "Pid", 3) == 0)
            {
                strncpy(pid_str, value, sizeof(pid));
            }
            else if (strncmp(field, "PPid", 4) == 0)
            {
                strncpy(ppid, value, sizeof(ppid));
            }
            else if (strncmp(field, "State", 5) == 0)
            {
                strncpy(state, value, sizeof(state));
            }
            else if (strncmp(field, "VmRSS", 5) == 0)
            {
                strncpy(mem, value, sizeof(mem));
            }
            else if (strncmp(field, "Threads", 7) == 0)
            {
                strncpy(threads, value, sizeof(threads) - 1);
            }
            // Check if this is even a user's process
            else if (strncmp(field, "Uid", 3) == 0)
            {
                if (strtol(value, NULL, 10) != user_id)
                    return 0;
            }
        }
    }

    printf("%-7s %-7s %-20s %-15s %-12s %-4s\n", pid_str, ppid, name, state, mem, threads);

    fclose(proc_file);

    return 0;
}

// Provided Helper Function:
// Check if a string is only digits
static int digits_only(const char *s)
{
    if (*s == '\0') return 0;
    for (; *s != '\0'; ++s)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

// Provided Helper Function:
// Get a directory entry and if its name is completely
// numeric, then it's a PID. 
static void show_candidates(uid_t user_id)
{
    // Open /proc directory
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir /proc");
        return;
    }

    // Print only numerical directory entries
    for (;;) {
        errno = 0;  /* Reset immediately before each readdir call. */
        struct dirent *entry = readdir(dir);
        if (entry == NULL) 
        {
            if (errno != 0) perror("readdir /proc");
            break;
        }
        if (digits_only(entry->d_name))
        {
            // Get the proc's information
            long pid = strtol(entry->d_name, NULL, 10);
            get_proc_status(user_id, pid);
        }
    }
    if (closedir(dir) == -1) perror("closedir");
}

void print_user_procs(void)
{
    uid_t user_id = getuid();

    // Get system time
    time_t now = time(NULL);
    struct tm *local_tm = localtime(&now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", local_tm);

    // Print header
    printf("procmon | UID: %d | Updated: ", user_id);
    printf("Formatted time: %s", buffer);
    printf(" | Refresh: 2 s\n");
    printf("PID     PPID    NAME                 STATE           RSS (KiB)    THREADS\n");

    // Print user-process information
    show_candidates(user_id);

}