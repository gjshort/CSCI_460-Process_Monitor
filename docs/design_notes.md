# Design Notes

## Notes
- In this assignment, “currently running processes” means processes currently present in /proc, including sleeping and stopped processes—not only processes executing on a CPU.
- Numbered directories such as /proc/1250/ represent processes.
- Non-numbered entries include system information such as cpuinfo, meminfo, and uptime.
- Every user account has a numeric user ID (UID), and every process carries UIDs recording whose behalf it runs on.
- UID 0 is root; ordinary accounts usually start at 1000 on current Linux distributions.

```bash
ls -l /usr/bin/passwd   # -rwsr-xr-x 1 root root ...
```

Reading the permission string. The first character is the file type (- means a regular file). The next nine characters are three groups of three, giving the permissions of the file's owner (rws), its group (r-x), and everyone else (r-x). In each group, r means read, w means write, and x means execute.

## Program Design

- Print matching-user processes
