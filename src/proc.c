#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "proc.h"

static void print_status_header()
{
    printf("PID     PPID    NAME                 STATE           RSS (KiB)    THREADS\n");
}

// Prints select info from /proc/<PID>/stat
// Filters by UID
// Field sizes based on Linux Man Pages: 
// https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html
static int print_proc_stat(uid_t user_id, long pid)
{
    // Create file path to /proc/pid/status
    char path[128];
    int n = snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
    if (n < 0 || (size_t)n >= sizeof(path))
    {
        perror("Invalid /proc/<pid>/stat path");
        return -1;
    }
    
    // Open proc's stat file
    FILE *proc_file = fopen(path, "r");
    if (proc_file == NULL)
    {
        perror("Could not open /proc/PID/stat for some process");
        return -1;
    }

    // Parse /proc/PID/status fields
    char buffer[1024];
    
    // Copy the 1-line entry of stat into the buffer
    fgets(buffer, sizeof(buffer), proc_file);

    // Record Parsing variables
    char *token;
    int num_fields = 10;

    long field_pid;
    char name[16];
    char state;
    int ppid;
    unsigned long utime;
    unsigned long stime;
    long priority;
    long nice;
    long num_threads;
    unsigned long long starttime;

    // Parse PID
    token = strtok(buffer, " ");
    if (token != NULL)
    {
        if (sscanf(token, "%ld ", &field_pid) == 1)
        {
            printf("PID: %ld\n", field_pid);
        }
        else { goto error; }
    }
    else { goto error; }

    // Parse process name (could contain spaces or parentheses).
    // Comes wrapped in parentheses
    token = strtok(NULL, " ");
    if (token != NULL)
    {
        token[strlen(token) - 1] = '\0'; // Removing ending parentheses
        if (sscanf(token, "(%15[^\n]", name) == 1) // Allow whitespace except newline
        {
            printf("Name: %s\n", name);
        }
        else { goto error; }
    }
    else { goto error; }
    
    // ---- TODO ---- 
    // Parse remainder of record on whitespace and string skipping
    int scan_match = sscanf(token, " %c %d %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu %*s %*s %ld %ld %ld %*s %llu %*s", 
                            &state, &ppid, &utime, &stime, &priority, &nice, &num_threads, &starttime);
    
    if (scan_match == num_fields - 2)
    {
        printf("%c %d %lu %lu %ld %ld %ld %llu\n", state, ppid, utime, stime, priority, nice, num_threads, starttime);
    }
    else 
    {
        printf("%d", scan_match); 
        goto error; 
    }

    goto ret;

error:
    printf("Error parsing /proc/%ld/stat\n", pid);
    return -1;

ret:
    fclose(proc_file);
    return 0;

} 


/**
* Collects information about a process from /proc/PID/status
* and prints it to the terminal. Only processes with a UID
* matching user_id will be considered.
*/
static int print_proc_status(uid_t user_id, long pid)
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
// Used by qsort
static int comp_ints(const void *n1, const void *n2)
{
    long x = *((long*)n1);
    long y = *((long*)n2);

    if (x > y) return 1;
    if (x < y) return -1;
    return 0;
}

// Modified Helper Function:
// Get a directory entry in /proc and if its name is completely
// numeric, then it's a PID. For PIDs with UID = user_id, 
// print PID information.
static void print_user_pid_statuses(uid_t user_id)
{
    // Open /proc directory
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir /proc");
        exit(EXIT_FAILURE);
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
        print_proc_status(user_id, pid_arr[i]);
    }

    free(pid_arr);
}


// Chcecks if given PID is present in /proc
bool pid_in_proc(long pid)
{
    bool retval = false;

    // Open /proc directory
    DIR *dir = opendir("/proc");
    if (dir == NULL) 
    {
        perror("opendir /proc");
        exit(EXIT_FAILURE);
    }

    for (;;)
    {
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
            // If PID dir matches requested PID
            long pid_dir = strtol(entry->d_name, NULL, 10);
            if (pid_dir == pid)
            {
                retval = true;;
                break;
            }
        }
    }
    if (closedir(dir) == -1) perror("closedir");

    return retval;
}


// FOR LIVE UPDATES
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
    print_status_header();

    // Print user-process information
    print_user_pid_statuses(user_id);

}


// FOR command: status <PID>
// Filters by UID
// Display select contents of /proc/PID/status
void print_pid_status(uid_t user_id, long pid)
{
    if (pid_in_proc(pid))
    {
        // Print select info in /proc/PID/status
        print_status_header();
        print_proc_status(user_id, pid);
    }
    else
    {
        printf("PID %ld not in /proc\n", pid);
    }

}


// FOR command: stat <PID>
// Filters by UID
// Display select contents of /proc/PID/stat
void print_pid_stat(uid_t user_id, long pid)
{
    if (pid_in_proc(pid))
    {
        // Print select info in /proc/PID/stat
        //print_status_header();
        print_proc_stat(user_id, pid);
    }
    else
    {
        printf("PID %ld not in /proc\n", pid);
    }

}