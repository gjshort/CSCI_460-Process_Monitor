#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "proc.h"

/**
* Collects information about a process from /proc/PID/status
* and prints it to the terminal. Only processes with a UID
* matching user_id will be considered.
*/
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
        perror("Could not open /proc/PID/status for some process");
        return -1;
    }

    // Parse /proc/PID/status fields
    char buffer[4096];
    char field[36];
    char value[36];

    char name[24];
    strncpy(name, "N/A", 4);
    char pid_str[24];
    strncpy(pid_str, "N/A", 4);
    char ppid[24];
    strncpy(ppid, "N/A", 4);
    char state[24];
    strncpy(state, "N/A", 4);
    char mem[24];
    strncpy(mem, "N/A", 4);
    char threads[24];
    strncpy(threads, "N/A", 4);

    while (fgets(buffer, sizeof(buffer), proc_file) != NULL)
    {
        int scan_ret = sscanf(buffer, "%35[^:]: %35[^, \n]", field, value);
        if (scan_ret == 2)
        {
            if (strncmp(field, "Name", 5) == 0)
            {
                strncpy(name, value, sizeof(name));
            }
            else if (strncmp(field, "Pid", 4) == 0)
            {
                strncpy(pid_str, value, sizeof(pid));
            }
            else if (strncmp(field, "PPid", 5) == 0)
            {
                strncpy(ppid, value, sizeof(ppid));
            }
            else if (strncmp(field, "State", 6) == 0)
            {
                strncpy(state, value, sizeof(state));
            }
            else if (strncmp(field, "VmRSS", 6) == 0)
            {
                strncpy(mem, value, sizeof(mem));
            }
            else if (strncmp(field, "Threads", 8) == 0)
            {
                strncpy(threads, value, sizeof(threads) - 1);
            }
            // Check if this is even a user's process
            else if (strncmp(field, "Uid", 4) == 0)
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

// Compares two integers. Returns:
// Negative int for *n1 < *n2
// Zero for *n1 = *n2
// Positive int for *n1 > *n2
static int comp_ints(const void *n1, const void *n2)
{
    long x = *((long*)n1);
    long y = *((long*)n2);

    if (x > y) return 1;
    if (x < y) return -1;
    return 0;
}

// Provided Helper Function:
// Get a directory entry in /proc and if its name is completely
// numeric, then it's a PID. For PIDs with UID = user_id, 
// print PID information.
static void show_candidates(uid_t user_id)
{
    // Open /proc directory
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir /proc");
        return;
    }

    // List of PIDs to be sorted and printed
    long *pid_arr = NULL;
    long *tmp_ptr = NULL;
    int num_procs = 0;

    // Look only for numerical directory entries in /proc
    for (;;) {
        errno = 0;  /* Reset immediately before each readdir call. */
        struct dirent *entry = readdir(dir);
        if (entry == NULL) 
        {
            if (errno != 0) perror("readdir /proc");
            break;
        }
        // If dir is a PID
        if (digits_only(entry->d_name))
        {
            // Expand PID list
            num_procs++;
            tmp_ptr = realloc(pid_arr, num_procs * sizeof(long));
            if (tmp_ptr == NULL)
            {
                perror("Could not allocate PID array");
                if (pid_arr != NULL)
                {
                    free(pid_arr);
                }
                return;
            }
            else
            {
                // Add to PID list
                pid_arr = tmp_ptr;
                long pid = strtol(entry->d_name, NULL, 10);
                pid_arr[num_procs -1] = pid;
            }

        }
    }
    if (closedir(dir) == -1) perror("closedir");

    // Sorting the PID array w/ num_procs entries of longs
    qsort(pid_arr, num_procs, sizeof(long), comp_ints);

    // Print PID info from sorted list
    for (int i = 0; i < num_procs; i++)
    {
        get_proc_status(user_id, pid_arr[i]);
    }

    free(pid_arr);
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
    printf("\nprocmon | UID: %d | Updated: ", user_id);
    printf("%s", buffer);
    printf(" | Refresh: 2 s\n");
    printf("PID     PPID    NAME                 STATE           RSS (KiB)    THREADS\n");

    // Print user-process information
    show_candidates(user_id);

}