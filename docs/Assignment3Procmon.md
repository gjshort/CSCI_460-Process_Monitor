# Programming Assignment: Explore Linux Processes Through /proc

**Language:** C (C11 with POSIX interfaces)  
**Due:** December 1, 2026, 11:59 PM (Always check Canvas on updated due dates)  
**Program name:** `procmon`  
**Total:** 100 points

## 1. Purpose

Create an interactive command-line program that displays information about the current user's processes and selected system information. Your program must obtain this information directly from Linux's `/proc` filesystem.

After completing this assignment, you should be able to:

1. Enumerate directory entries and distinguish process directories from other `/proc` entries.
2. Read kernel-provided information through file interfaces, parsing by **field label** rather than fixed line or position where the format allows.
3. Distinguish a process from its threads, and relate PID, TID, and thread-group ID.
4. Interpret process state, memory (RSS), and CPU-time information, and label units correctly.
5. Handle information that changes — or disappears — while your program is reading it, without crashing or leaking resources.

In this assignment, “currently running processes” means processes currently present in `/proc`, including sleeping and stopped processes—not only processes executing on a CPU.

## 2. Background and scope

`/proc` is a virtual filesystem: its files generally expose kernel-maintained information rather than ordinary disk contents. Numbered directories such as `/proc/1250/` represent processes. Non-numbered entries include system information such as `cpuinfo`, `meminfo`, and `uptime`.

Read all information directly using C/POSIX interfaces. Do not invoke `ps`, `top`, `cat`, `ls`, `free`, or similar utilities from your program. Do not use `system()` or `popen()` to implement features. You may run these utilities manually when checking your results.

Run as an ordinary user; do not require `sudo`. This is a read-only monitor: do not modify `/proc` entries or implement process termination.

For this assignment, a process belongs to the current user when the **real UID** in its `status` file equals the monitor's `getuid()` result. Apply this rule to both the process list and commands targeting a PID. Use the first number on the `Uid:` line, not the username, directory name, or effective UID. This is an explicit assignment convention; real and effective UIDs can differ.

### Understanding user IDs (try these before you start)

The kernel does not identify users by name. Every user account has a numeric **user ID (UID)**, and every process carries UIDs recording whose behalf it runs on. Usernames such as `alice` are a user-space convenience: tools translate a UID to a name by looking it up in the account database (`/etc/passwd`, or a network directory on some lab machines). UID 0 is `root`; ordinary accounts usually start at 1000 on current Linux distributions.

Run the following as your ordinary user in a terminal. The ownership rule above and the permission-denied handling in Section 3.B both depend on what these steps show.

1. **Your identity.**
   ```bash
   id          # uid=1000(alice) gid=1000(alice) groups=...
   id -u       # just the number: this is what getuid() returns
   id -un      # just the name
   ```

2. **Where the name comes from: `getent`.** `getent` ("get entries") looks up a record in one of the system's account databases. Its first argument names the database and the second is the key to look up:
   ```bash
   getent passwd "$(id -un)"   # look up by name:   alice:x:1000:1000:Alice:/home/alice:/bin/bash
   getent passwd "$(id -u)"    # look up by number: same record
   getent passwd 0             # root:x:0:0:root:/root:/bin/bash
   getent group "$(id -g)"     # your primary group: alice:x:1000:
   ```
   A `passwd` record has seven colon-separated fields: username, a password placeholder (`x` means the real hash is kept elsewhere, in `/etc/shadow`), **UID**, primary group ID (GID), a comment field (often the full name), home directory, and login shell.

   Why `getent` rather than `cat /etc/passwd`? `/etc/passwd` is only *one* possible source. The file `/etc/nsswitch.conf` tells the system which sources to consult for each database (look at its `passwd:` line). On many university lab machines your account comes from a network directory service such as LDAP, so it does not appear in `/etc/passwd` at all. `getent` queries the same sources, in the same order, that programs see through the C library, so it always shows the answer your programs will get.

   **How this relates to `/proc`:** the kernel never stores usernames. `/proc/PID/status` contains only numbers (the `Uid:` and `Gid:` lines). Every tool that shows a name, such as `ps`, `ls -l`, or `top`, reads that number and then performs the same kind of lookup `getent` does. In C this is `getpwuid()` (`<pwd.h>`). Compare:
   ```bash
   grep -E '^(Uid|Gid):' /proc/$$/status   # numbers only
   getent passwd "$(grep '^Uid:' /proc/$$/status | cut -f2)"   # number -> account record
   ```
   For this assignment, compare numbers, never names. Displaying a username is optional; if you add it, use `getpwuid()` and fall back to the number when no account record exists.

3. **A process's UIDs in `/proc`.** `$$` expands to your shell's PID:
   ```bash
   grep Uid /proc/$$/status    # Uid:  1000  1000  1000  1000
   ```
   The four numbers are the **real**, **effective**, **saved-set**, and **filesystem** UIDs. For an ordinary process all four are normally equal.

4. **Names are a display choice; numbers are what the kernel stores.**
   ```bash
   ls -ld  /proc/$$    # owner shown as a name
   ls -ldn /proc/$$    # same entry, owner shown as a number
   ```

5. **Other users' processes.** Many system processes run as root (UID 0) or as service accounts:
   ```bash
   ps -eo pid,ruid,euid,ruser,comm | head -20
   ```
   `RUID` and `EUID` are the real and effective UIDs.

6. **Seeing real and effective UIDs differ.** A *setuid* program runs with the effective UID of the file's owner while keeping the real UID of the user who started it. `passwd` is a common example; look for the `s` in its permissions:
   ```bash
   ls -l /usr/bin/passwd   # -rwsr-xr-x 1 root root ...
   ```
   **Reading the permission string.** The first character is the file type (`-` means a regular file). The next nine characters are three groups of three, giving the permissions of the file's **owner** (`rws`), its **group** (`r-x`), and **everyone else** (`r-x`). In each group, `r` means read, `w` means write, and `x` means execute.

   The `s` sits where the owner's `x` would normally be. It means two things: the owner may execute the file, **and** the file's **setuid bit** is set. When any user runs a setuid file, the new process gets the file owner's UID (here, root's UID 0) as its **effective** UID, while its **real** UID remains the UID of the user who ran it. An uppercase `S` would mean the setuid bit is set but execute permission is not, which is usually a misconfiguration.

   `passwd` needs this because it must update the password database (`/etc/shadow`), which ordinary users cannot write. Setuid lets the program act with root's privileges while still knowing, through the real UID, which user's password to change. You can see the bit numerically too:
   ```bash
   stat -c '%A %a' /usr/bin/passwd   # -rwsr-xr-x 4755  (the leading 4 is the setuid bit)
   ls -l /etc/shadow                 # owned by root; not writable by you
   ls -ld /tmp                       # drwxrwxrwt ... root root ... /tmp
   stat -c '%A %a' /tmp              # drwxrwxrwt 1777  (the leading 1 is the sticky bit)
   ```
   Two related bits use the same execute slots. An `s` in the **group** position is the *setgid* bit: a setgid program runs with the file's group as its effective group ID.

   A `t` in the **others** position is the *sticky* bit. Look at the `/tmp` output: the leading `d` means it is a directory, and `rwxrwxrwx` would let every user create, rename, and delete any file in it. That is what `/tmp` needs, since every user and program writes temporary files there, but it would also let any user delete or replace anyone else's files. The sticky bit closes that gap: in a sticky directory, a file can be deleted or renamed only by the file's owner, the directory's owner, or root. Everyone can still create files in `/tmp`; they just cannot remove each other's. As with `s` and `S`, a lowercase `t` means others also have execute (search) permission on the directory, and an uppercase `T` means they do not. On Linux the sticky bit matters only on directories; the kernel ignores it on regular files.

   You will meet `/tmp` again later: the supplied `memory_threads` example calls `tmpfile()`, which typically creates its temporary file there (Section 11, test 3).

   In **terminal A**, run `passwd` and **stop at the first password prompt — do not type a password.** In **terminal B**:
   ```bash
   pid=$(pgrep -n -U "$(id -u)" passwd)    # -U matches the REAL UID
   grep Uid /proc/$pid/status              # e.g. Uid:  1000  0  0  0
   ls -ldn /proc/$pid                      # directory owner is typically 0, not 1000
   ps -o pid,ruid,euid,comm -p "$pid"
   cat /proc/$pid/maps                     # typically: Permission denied
   ```
   Then press Ctrl+C in terminal A to cancel `passwd` without changing anything. If your system's output differs (distributions install `passwd` differently), describe what you observed instead.

**What this means for `procmon`.** The real UID answers "who started this process," which is the question this assignment's ownership rule asks. The `/proc/PID` directory owner reflects the effective UID (and the kernel may also assign it to root for protected processes like `passwd`), so it is not a reliable stand-in, which is why you must read the first value on the `Uid:` line. Step 6 also shows why Section 3.B warns that same-user ownership does not guarantee access: under the assignment's rule, the `passwd` process belongs to you and should appear in your list, yet the kernel may deny some of its files. Your monitor must report that denial clearly rather than crash or display nothing.

## At a glance: what is graded

The sections that follow mix hard requirements with advisory guidance and optional extensions. Use this map to tell them apart; section numbers point to the full details, and the right column to the matching rubric row (Section 12).

**Required — must work for full credit**

- Enumerate `/proc` and filter to the current user's processes by **real UID** (§2, §4) — *15 pts*
- Live table: PID, PPID, Name, State, RSS (KiB), Threads; numeric PID sort; header with UID, timestamp, and count; ~2 s refresh; `N/A` for missing fields (§3.A) — *15 pts*
- Two-mode interface: auto-refreshing live view + paused command mode driven by `poll()` (§8) — *15 pts*
- Six PID detail commands — `status`, `stat` (labeled subset, §6), `cmdline` (null-separated), `maps` (with a format heading), `fd` (link targets via `readlink`), `tasks` (TID/name/state) (§3.B, §6) — *25 pts*
- System/utility commands — `cpuinfo`, `meminfo`, `uptime` (both values labeled and formatted), plus `help`, `list`, `quit` (§3.B, §7) — *10 pts*
- Input validation (`strtol`, reject nonpositive/overflow/junk/missing/extra args), per-command UID recheck, resource cleanup, and graceful handling of vanished or access-denied entries (§3.B, §9) — *10 pts*
- Build command or Makefile, README, test report, and the conceptual answers (§10–§12) — *10 pts*

**Required constraints — violating these loses credit regardless of features**

- Read `/proc` directly; do **not** shell out to `ps`, `top`, `cat`, `ls`, `free`, `system()`, or `popen()` (§2).
- Run as an ordinary user; read-only — never modify `/proc` or terminate processes (§2).
- Match field **labels**, use **checked** numeric conversions, and keep all buffer operations **bounded** (§5).
- **Escape control characters (especially ESC)** in any process-supplied string before printing it (§5).
- For `fd`, inspect link targets with `readlink`; do **not** open or read the targets (§6).

**Recommended — the way to earn the robustness and cleanup points**

- Prefer the `FILE *` stdio approach (Approach A); close and reopen each `/proc` file every refresh; never loop on `feof` or size files with `stat().st_size` (§5).
- Collect all rows, then sort and render; keep error messages concise and off the live table (§9).

**Optional — not required for full credit (see Section 12)**

- Name filter, memory sort, pagination, adjustable interval (§12); interval CPU-utilization sampling and PID-reuse detection via `starttime` (§9, §12).

## 3. Required behavior

### A. Live process list

At startup, display a table containing only matching-user processes, sorted numerically by PID:

| Column | Source in `/proc/<pid>/status` |
| --- | --- |
| PID | `Pid:` |
| Parent PID | `PPid:` |
| Name | `Name:` |
| State | `State:` |
| Resident memory, KiB | `VmRSS:` |
| Thread count | `Threads:` |

Refresh approximately every two seconds by rescanning `/proc` and rereading the relevant files. New processes should appear and removed processes should disappear. Show a refresh timestamp, current UID, and number of displayed processes. Display `N/A` for unavailable optional fields rather than inventing zero values. If ownership cannot be established, skip the process.

Example layout (illustrative values):

```text
procmon | UID: 1000 | Updated: 14:32:08 | Refresh: 2 s
PID    PPID   NAME         STATE          RSS (KiB)   THREADS
1250   1100   bash         S (sleeping)   4200        1
1400   1250   procmon      R (running)    1800        1

Processes shown: 2
Press Enter to pause and enter a command.
```

### B. Commands

Pressing Enter in live mode must pause automatic refresh and open a command prompt. Commands are entered as complete lines followed by Enter. Detail views remain visible until the next command; `list` resumes the live table.

| Command | Required result |
| --- | --- |
| `help` | Explain commands and interaction modes. |
| `list` | Resume the automatically refreshing process list. |
| `status PID` | Display the readable contents of `/proc/PID/status`. |
| `stat PID` | Display the labeled subset of fields specified in Section 6. |
| `cmdline PID` | Display the command-line arguments with null separators made readable. |
| `maps PID` | Display memory mappings with a column heading explaining their format. |
| `fd PID` | List descriptor numbers and symbolic-link targets. |
| `tasks PID` | List each thread's TID, name, and state. |
| `cpuinfo` | Display `/proc/cpuinfo`. |
| `meminfo` | Display `/proc/meminfo`. |
| `uptime` | Label and display both values from `/proc/uptime`; also format uptime as days/hours/minutes/seconds. |
| `quit` | Release resources and exit. |

For every PID command, validate the PID and check its real UID before displaying information. Reject nonpositive numbers, trailing junk, overflow, missing arguments, and extra arguments. Use checked numeric conversion such as `strtol()`, not `atoi()`. Do not accept arbitrary paths as PID arguments.

An invalid command must produce a helpful message without terminating the monitor. If a process vanishes, report that it is no longer available. If access is denied, report that separately. Do not assume same-user ownership guarantees access to every file. System-wide commands are not filtered by UID.

Long detail output may use normal terminal scrollback; pagination is optional. Do not clear it immediately after printing it.

## 4. Enumerating /proc directories

Use `<dirent.h>` and these library functions:

```c
DIR *dir = opendir("/proc");
struct dirent *entry = readdir(dir);
int result = closedir(dir);
```

`opendir()` returns `NULL` on failure. Each successful `readdir()` returns an entry whose `d_name` is its name. A `NULL` result means end of directory or an error; distinguish these using `errno`.

The following helper demonstrates the enumeration technique. It prints candidate PID names; you must add ownership filtering, information collection, and sorting.

```c
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>

static int digits_only(const char *s)
{
    if (*s == '\0') return 0;
    for (; *s != '\0'; ++s)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static void show_candidates(void)
{
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir /proc");
        return;
    }

    for (;;) {
        errno = 0;  /* Reset immediately before each readdir call. */
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) perror("readdir /proc");
            break;
        }
        if (digits_only(entry->d_name))
            printf("%s\n", entry->d_name);
    }
    if (closedir(dir) == -1) perror("closedir");
}
```

Important practices:

- Scan actual entries; do not try every integer from 1 to a guessed maximum PID.
- Validate the entire name, not just its first character.
- Do not assume directory enumeration is sorted.
- Do not rely solely on `d_type`; it can be `DT_UNKNOWN`. For numbered candidates, attempting to open the expected file and handling failure is sufficient here.
- `readdir()` manages the returned storage. Copy a name if you need it after subsequent calls; do not free the returned entry.
- Rescan each refresh. Never assume yesterday's—or the previous refresh's—process list remains valid.
- Use `opendir()`/`readdir()` for directories, not `fopen()`/`fgets()`. Raw `getdents` system calls are unnecessary for this assignment.

### Why "all digits" identifies process directories

The kernel names each process's directory with its PID, written in decimal with no leading zeros, so every process directory name consists only of digits. No process ever gets a non-digit name. Every non-digit entry in `/proc` is something else. See for yourself:

```bash
ls /proc | grep -v '^[0-9]*$'     # cpuinfo, meminfo, uptime, sys, net, self, ...
ls -l /proc/self /proc/thread-self
```

Two of those deserve attention:

- `self` is a symbolic link to the directory of *whichever process is looking*, and `thread-self` links to the looking thread's `task/TID` directory. Your digit check already skips both, which is correct: following `self` would list your monitor a second time.
- **Threads are deliberately hidden.** A worker thread's TID is a number too, and `/proc/TID/status` can be opened directly, but the kernel does **not** return TIDs of non-main threads from `readdir("/proc")`. Only thread-group leaders (processes) are listed, which is why your table shows one row per process. To see threads, enumerate `/proc/PID/task/` (Section 6).

The digit check is therefore correct and sufficient here. You may see *fewer* processes than exist (for example, a system mounting `/proc` with the `hidepid` option, or a container with its own PID namespace, hides other processes), but you will never miss a process because of its name.

### Learning more about `struct dirent`

`readdir()` returns a pointer to a `struct dirent`. Run `man 3 readdir` (or `man readdir`) to see the structure's definition. The page describes its fields, including `d_name` (the entry's name, which you use here), `d_ino` (inode number), and `d_type` (entry type, which may be `DT_UNKNOWN`). It also explains which fields POSIX guarantees and which are Linux extensions. If `man` reports that no page exists, the development man pages are not installed. Use the online readdir(3) link in Section 13; on your own Debian/Ubuntu machine you can install them with `sudo apt install manpages-dev`.

## 5. Reading files: two acceptable approaches

### Approach A: standard I/O with FILE pointers (recommended)

Using `fopen()` on readable `/proc` files is appropriate and encouraged. It is not a bad practice. The C library ultimately uses operating-system file operations; it additionally provides buffering and convenient text functions.

```c
#include <stdio.h>

static int show_text_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof buffer, fp) != NULL)
        fputs(buffer, stdout);

    int failed = ferror(fp);
    if (failed) fprintf(stderr, "Read failed: %s\n", path);
    if (fclose(fp) == EOF) failed = 1;
    return failed ? -1 : 0;
}
```

This example copies text in chunks. For parsing complete lines, use POSIX `getline()` (and free its allocated buffer), or explicitly handle lines longer than a fixed buffer. Match field labels such as `Uid:` and `VmRSS:`; do not assume a field always appears on a fixed line number. Check the number of successful conversions returned by `sscanf()`.

Construct paths with `snprintf()` and check its return value:

```c
char path[128];
int n = snprintf(path, sizeof path, "/proc/%ld/status", pid);
if (n < 0 || (size_t)n >= sizeof path) {
    /* Report invalid/truncated path; do not use it. */
}
```

Here `pid` is a previously validated positive `long`.

### Approach B: file descriptors and system-call interfaces

You may instead use `open()`, `read()`, and `close()` through their usual C library wrappers. You do not need to call `syscall()` directly.

```c
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int show_bytes(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) { perror(path); return -1; }
    char buffer[4096];
    int failed = 0;

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof buffer);
        if (n > 0) {
            if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n) {
                failed = 1;
                break;
            }
        } else if (n == 0) {
            break;  /* EOF */
        } else if (errno != EINTR) {
            perror("read");
            failed = 1;
            break;
        }
    }
    if (close(fd) == -1) failed = 1;
    return failed ? -1 : 0;
}
```

`read()` returns a byte count, zero for EOF, or -1 for an error. Short reads are valid. It does not append a string terminator. This example uses a byte count rather than treating the buffer as a C string. If you use `write()` instead of `fwrite()`, handle short writes and interrupted writes too.

**The `O_RDONLY` argument and related flags.** The second argument to `open()` is an integer built from constants defined in `<fcntl.h>`. It must contain **exactly one access mode**:

| Access mode | Meaning |
| --- | --- |
| `O_RDONLY` | Open for reading only |
| `O_WRONLY` | Open for writing only |
| `O_RDWR` | Open for reading and writing |

You may then combine the access mode with optional flags using bitwise OR (`|`), for example `open(path, O_RDONLY | O_CLOEXEC)`. Common flags:

| Flag | Effect |
| --- | --- |
| `O_CLOEXEC` | Close this descriptor automatically if the process later calls `exec()`, so it does not leak into a new program. A good habit. |
| `O_NONBLOCK` | Calls that would wait return immediately with an error (`EAGAIN`) instead of blocking. |
| `O_NOFOLLOW` | Fail if the final path component is a symbolic link. |
| `O_DIRECTORY` | Fail unless the path is a directory. |
| `O_CREAT` | Create the file if it does not exist; requires a third argument giving its permissions, e.g. `0644`. |
| `O_TRUNC` | Truncate an existing file to length zero when opening it for writing. |
| `O_APPEND` | Make every write go to the end of the file. |

The access modes are not independent bits: `O_RDONLY` is usually defined as 0, so `O_RDONLY | O_WRONLY` does **not** mean "read and write"; use `O_RDWR`. In C stdio terms, `fopen(path, "r")` corresponds to `O_RDONLY`, `"w"` to `O_WRONLY | O_CREAT | O_TRUNC`, and `"a"` to `O_WRONLY | O_CREAT | O_APPEND`.

This monitor is read-only, so `O_RDONLY` (optionally with `O_CLOEXEC`) is the only mode you need. Never open a `/proc` file with a writing mode: some `/proc` files are writable control interfaces, and writing them changes kernel or process behavior. See `man 2 open` for the full list.

### Practices to avoid

| Avoid | Use instead |
| --- | --- |
| `while (!feof(fp))` | Loop on the result of `fgets()` or `fread()`, then check `ferror()`. |
| Assuming `stat().st_size` gives readable content length | Read until EOF; many `/proc` files report size zero. |
| Using `fseek(..., SEEK_END)`/`ftell()` to allocate the whole file | Stream chunks or grow a buffer as needed. |
| `printf(buffer)` on file contents | `fputs(buffer, stdout)` or `printf("%s", buffer)`. |
| Unbounded `%s`, `strcpy()`, or unchecked `sprintf()` | Bounded operations and checked conversions. |
| Reusing an EOF-positioned stream for the next refresh | Close and reopen each file for a fresh sample. |
| Mixing `read(fileno(fp), ...)` with buffered reads on the same stream | Choose one input abstraction per opened file. |
| Closing `fileno(fp)` and then calling `fclose(fp)` | Close a `FILE *` with `fclose()` exactly once. |
| Checking existence first and assuming the later open succeeds | Attempt the open directly and handle failure. |
| Leaking resources on early returns | Close files/directories and free allocated memory on every path. |

These snippets illustrate I/O, not a complete monitor. Before rendering process-supplied strings, escape control characters, especially ESC, so a process name or argument cannot disrupt the terminal. Preserve intentional layout newlines in text views.

## 6. Understanding the process entries

### /proc/PID/status: labeled summary

Parse `Name`, `State`, `Pid`, `PPid`, `Uid`, `VmRSS`, and `Threads` for the live table and ownership check. The four `Uid` values are real, effective, saved-set, and filesystem UIDs, in that order (see "Understanding user IDs" in Section 2).

Common states include R (running or runnable), S (interruptible sleep), D (uninterruptible wait), T (stopped), and Z (zombie). Preserve unfamiliar state codes instead of rejecting them. A sleeping process still belongs in your table.

`VmRSS` describes resident memory, not the process's entire virtual address space; this counter is approximate. Linux reports these memory quantities with a `kB` label representing 1024-byte units. Label your table KiB. Missing memory fields should appear as `N/A`.

### /proc/PID/stat: positional statistics

For `stat PID`, parse and label these fields using their documented, one-based positions:

| Field | Meaning |
| --- | --- |
| 1: pid | Process ID |
| 2: comm | Parenthesized process name |
| 3: state | State code |
| 4: ppid | Parent PID |
| 14: utime | Accumulated user-mode CPU ticks |
| 15: stime | Accumulated kernel-mode CPU ticks |
| 18: priority | Kernel-reported scheduling priority value |
| 19: nice | Nice value |
| 20: num_threads | Thread count |
| 22: starttime | Start time in ticks since boot |

The name can contain spaces and parentheses. Do not split the whole record on whitespace or parse the name with a simple `%s`. Read the complete record, locate its opening `(` and final closing `)`, then tokenize the remaining fields beginning with field 3. Check token counts and conversions. The priority value is not a universal “larger means higher” score.

Convert CPU ticks to seconds using `sysconf(_SC_CLK_TCK)` from `<unistd.h>`; check for failure. Display `(utime + stime) / (double)ticks_per_second`. This is accumulated CPU time, not CPU utilization percentage.

### /proc/PID/cmdline: null-separated arguments

Arguments are normally separated by `\0` bytes. Use `fread()` or `read()` and process every returned byte; `strlen()` and `%s` would stop at the first separator. Display arguments separately or replace separators with visible spaces. Do not execute the resulting text. An empty file is possible; show `[empty or unavailable command line]` rather than crashing. Processes can modify the exposed argument text, so it is not authoritative proof of their executable identity.

**Shell notation used in the exploration examples.** The examples in this section and the next start short-lived test processes and then inspect them. They rely on four pieces of shell notation:

| Notation | Meaning |
| --- | --- |
| `command &` | Run the command **in the background**: the shell starts it and immediately gives you the prompt back instead of waiting for it to finish. The shell prints a job number and PID, such as `[1] 4123`. |
| `$!` | Expands to the **PID of the most recent background command**. It changes each time you start another `&` command, so save it (`pid=$!`) if you need it later. For a pipeline such as `a \| b &`, `$!` is the PID of the *last* command, `b`. |
| `$$` | Expands to the PID of **your shell itself** (used in Section 2). Unlike `$!`, it stays the same for the whole session. |
| `kill PID` | Send a **signal** to that process. With no option, `kill` sends `SIGTERM` ("please terminate"), which ends a program like `sleep` immediately. `kill $!` therefore means "terminate the background process I just started." `kill %%` targets the most recent background *job*; for a pipeline, that is every process in it. |

The examples always end with a `kill` so the test processes do not keep running for 300 seconds. When a background job ends, the shell may print a line such as `[1]+  Terminated  sleep 300` the next time it shows a prompt. That message is expected. As an ordinary user, `kill` can signal only your own processes, so it cannot harm other users' processes. Sending signals to your own test processes from the shell is fine; your monitor itself must never terminate processes (Section 2).

**Explore it from the shell first.** Start a process with arguments and look at its command line three ways:

```bash
sleep 300 &                      # $! now holds the PID of this background job
pid=$!
cat /proc/$pid/cmdline; echo     # prints "sleep300": the NUL bytes are invisible
tr '\0' ' ' < /proc/$pid/cmdline; echo   # NULs replaced by spaces: "sleep 300 "
od -c /proc/$pid/cmdline         # shows every byte: s l e e p \0 3 0 0 \0
kill $pid
```

`cat` is not hiding anything. The terminal simply displays NUL bytes as nothing, which is exactly why a C program that uses `strlen()` or `%s` would stop after `sleep`. `od -c` ("octal dump, as characters") is the most honest view: the arguments are `sleep` and `300`, each followed by a terminating `\0`. To see an argument that contains a space, start a process that stays alive with such an argument:

```bash
tail -f /dev/null 'two words' 2>/dev/null &   # tail keeps following /dev/null
od -c /proc/$!/cmdline     # t a i l \0 - f \0 / d e v / n u l l \0 t w o   w o r d s \0
kill $!
```

The space inside `two words` is preserved as an ordinary byte; only `\0` separates arguments. (`tail` complains that the file `two words` does not exist, which is harmless here; `2>/dev/null` hides the message.)

### /proc/PID/maps: virtual memory mappings

**What `maps` shows.** Every process has its own *virtual address space*, the range of memory addresses its program can use. The kernel divides that space into **regions** (also called *mappings* or *virtual memory areas*). `/proc/PID/maps` lists every region the process currently has, one per line, in order of increasing address. The program's own code and data, each shared library, the heap, the main thread's stack, the stacks of other threads, memory from large `malloc()` calls, and small kernel-provided areas such as `[vdso]` all appear here. Together they are a map of the process's address space. Addresses not covered by any line are unmapped, and touching one causes a segmentation fault.

A typical layout, from low addresses to high:

```text
low   program code and data        (/usr/bin/sleep, several lines)
      [heap]                       grows upward as malloc() needs more
      ... large unmapped gap ...
      shared libraries (libc.so.6, ld-linux...), anonymous regions,
      thread stacks, [vvar] and [vdso] (small kernel-provided pages)
      [stack]                      main thread's stack, grows downward
high  [vsyscall]                   legacy kernel page; only on some x86-64 systems
```

The exact placement varies between runs, because Linux randomizes addresses (*address space layout randomization*) to make attacks harder. Run the same program twice and the addresses differ.

Two limits are worth knowing. First, `maps` shows *virtual* regions: what addresses exist and what they may be used for, not where they are in physical RAM. Second, a region's size is not memory actually in use. A region is *reserved address space*, and the kernel supplies physical pages only when the program first touches them. A 32 MiB region contributes nothing to RSS until the program writes to it, which is why the supplied `memory_threads` example writes every byte. To see how much of each region is actually resident, look at the `Rss:` lines in `/proc/PID/smaps`, the detailed version of the same list. You do not need to implement `smaps`.

Each line describes a virtual address range, followed by permissions, file offset, device, inode, and an optional pathname or label. Explain `r`, `w`, `x`, and the private/shared indicator `p`/`s` in your output heading or help. Labels may include `[heap]` and `[stack]`; some mappings have no name.

Display the lines in this assignment; a full parser is unnecessary. These are virtual memory regions, not a list of physical RAM addresses. Pathnames can contain spaces, so a future parser must not assume every whitespace-separated token is a separate field.

**Explore it from the shell first.**

```bash
sleep 300 &
pid=$!
cat /proc/$pid/maps
grep -E '\[(heap|stack|vdso)\]' /proc/$pid/maps
kill $pid
```

Reading one line field by field (your addresses and inode numbers will differ):

```text
55f532607000-55f53260b000 r-xp 00002000 fe:00 191059   /usr/bin/sleep
└──── address range ────┘ └perm┘└offset┘└dev┘ └inode┘   └ pathname ┘
```

- **Address range:** start and end of the region in the process's *virtual* address space, in hexadecimal. The size is end − start; this one is `0x4000` bytes = 16 KiB.
- **Permissions:** `r` read, `w` write, `x` execute, `-` not permitted; the last character is `p` (private: copy-on-write, changes are not shared) or `s` (shared with other processes).
- **Offset:** where in the file this region starts (here, 0x2000 bytes into `/usr/bin/sleep`).
- **Device and inode:** which file backs the region; `00:00 0` means no file.
- **Pathname:** the backing file, a label in brackets, or nothing.

Things to notice in the output: `/usr/bin/sleep` appears several times with different permissions. Its machine code is mapped `r-xp` (executable, not writable), while read-only data is `r--p`. That separation is a security property: code is normally not writable, so a bug cannot easily overwrite it. Shared libraries such as `libc.so.6` follow the same pattern. `[heap]` holds memory obtained by small `malloc()` calls, `[stack]` holds the main thread's stack, and `[vdso]` is a small kernel-provided page that lets some system calls such as `clock_gettime()` run without entering the kernel. Lines with no pathname are **anonymous mappings**: memory not backed by any file, such as large `malloc()` allocations and the stacks of extra threads. You will see both in the supplied `memory_threads` workload (Section 11, test 3).

### /proc/PID/fd/: open descriptors

Enumerate the numeric entries and use `readlink()` to inspect each symbolic link. Targets can identify regular files, terminals, pipes, sockets, and other kernel objects.

```c
char target[4096];
ssize_t n = readlink(link_path, target, sizeof target - 1);
if (n == -1) {
    /* Handle permission denial or an entry that disappeared. */
} else {
    target[n] = '\0';  /* readlink does not add this. */
    /* If n == sizeof target - 1, mark possible truncation or retry larger. */
}
```

Use `<unistd.h>`. Display the descriptor number and target; escape control characters before printing the target. Do not open and read the target itself: it may be a pipe or device and reading it could block or consume data. Descriptor 0 conventionally represents standard input, 1 standard output, and 2 standard error, but these may be redirected or closed.

**Explore it from the shell first.** `/proc/PID/fd/` contains one entry per open descriptor, named by its number. Each entry is a symbolic link, and `ls -l` shows where it points after the `->` arrow. The three steps below start with your own shell, then create processes whose descriptors you control, so you know what the output should be before you look. (Shell notation such as `&`, `$!`, `$$`, and `kill` is explained in the `cmdline` subsection above.)

**Step 1: your shell's descriptors.**

```bash
ls -l /proc/$$/fd
#   0 -> /dev/pts/3     standard input:  your terminal
#   1 -> /dev/pts/3     standard output: your terminal
#   2 -> /dev/pts/3     standard error:  your terminal
```

`$$` is your shell's PID, so this lists the shell's own open descriptors. An interactive shell reads from and writes to the terminal window, so all three standard descriptors point to the same terminal device. Your number after `pts/` will differ: each terminal window gets its own.

**Step 2: redirect a process's standard streams, then look.**

```bash
sleep 300 < /etc/hostname > /tmp/out.txt 2>&1 &
ls -l /proc/$!/fd
#   0 -> /etc/hostname
#   1 -> /tmp/out.txt
#   2 -> /tmp/out.txt
kill $!
```

The first line starts one process and sets up its descriptors before `sleep` begins running. Read it piece by piece:

| Piece | What it does |
| --- | --- |
| `sleep 300` | The program to run. It does nothing for 300 seconds, so it stays alive long enough to inspect. It never actually reads its input or writes output; we only care about what its descriptors point to. |
| `< /etc/hostname` | Redirect **standard input** (descriptor 0) so it comes from the file `/etc/hostname` instead of the terminal. Any readable file would do. |
| `> /tmp/out.txt` | Redirect **standard output** (descriptor 1) to the file `/tmp/out.txt`. The shell creates the file, or empties it if it already exists. |
| `2>&1` | Redirect **standard error** (descriptor 2) to wherever descriptor 1 currently points, which is `/tmp/out.txt`. Read it as "make 2 a copy of 1." Order matters: it must come *after* `> /tmp/out.txt`; written before it, stderr would copy the old target (the terminal). |
| `&` | Run the whole command in the background, so you get the prompt back immediately while `sleep` keeps running. |

The second line, `ls -l /proc/$!/fd`, lists the descriptors of the process just started (`$!` is its PID). The output confirms each redirection: 0 points to the input file, and 1 and 2 both point to the output file. The third line, `kill $!`, terminates the `sleep`. You can also write the first two lines as one, `sleep 300 < /etc/hostname > /tmp/out.txt 2>&1 & ls -l /proc/$!/fd`, because `&` ends the first command just as a newline or `;` would.

The redirections are performed by the shell, not by `sleep`. Before starting the program, the shell opens the files and places them on descriptors 0, 1, and 2; the program simply inherits them. This is the same mechanism the supplied launcher `run_all.sh` uses to send each workload's output to a log file (Section 11, test 5).

**Step 3: a pipe.**

```bash
sleep 300 | sleep 300 &
ls -l /proc/$!/fd
#   0 -> pipe:[123456]   read end of the pipe
#   1 -> /dev/pts/3      the terminal, as in step 1
#   2 -> /dev/pts/3
kill %%
```

`a | b` starts two processes and connects them with a **pipe**, a kernel buffer with a write end and a read end. The shell attaches the first `sleep`'s standard output to the write end and the second `sleep`'s standard input to the read end. `$!` is the PID of the *last* command in the pipeline, the second `sleep`, so its descriptor 0 is the pipe. To see the other end, run `jobs -l` (before the `kill`). It lists both PIDs in the pipeline, the first `sleep` on the first line and the second on the next. Then run `ls -l /proc/FIRST_PID/fd`: its descriptor 1 shows `pipe:` with the **same number** as the second `sleep`'s descriptor 0, which is how you can tell the two ends belong to the same pipe. Finally, `kill %%` terminates the whole pipeline. `kill $!` would stop only the second `sleep`, and `kill $(jobs -p)` only the first.

What the targets mean:

- `/dev/pts/N` is a terminal device: each terminal window has its own number.
- A regular pathname is an open file. If the file has been deleted while still open, the target ends in ` (deleted)`; the data stays usable until the last descriptor closes.
- `pipe:[123456]` and `socket:[123456]` are kernel objects with no pathname; the number is an inode that identifies the object. Two processes showing the **same** pipe number are connected to each other.
- `anon_inode:[eventfd]` and similar are other kernel objects.

`readlink()` returns exactly the text that `ls -l` shows after the arrow. Notice what you should **not** run: `cat /proc/$$/fd/0` would open your terminal and wait for you to type, and reading a pipe's descriptor would steal data from the program that owns it. That is why your monitor must only inspect the links and never read their targets.

### /proc/PID/task/: threads

Enumerate its numeric TID directories. Read `/proc/PID/task/TID/status` and show `Pid` (the TID here), `Name`, and `State`. `Tgid` identifies the thread group, corresponding to the process PID. A single-threaded process normally has one entry; the main thread's TID equals the PID. Handle threads disappearing during enumeration. Counts observed in separate reads may differ because threads can start or exit between reads.

## 7. System information commands

`cpuinfo`: display the file without assuming an architecture-specific field layout or a single processor entry.

`meminfo`: display the file and preserve its labels and units. Explain in your help that `MemAvailable` estimates memory available for new work without swapping, whereas `MemFree` counts unused RAM.

`uptime`: parse two floating-point numbers. The first is elapsed uptime in seconds; the second is accumulated idle time across CPUs. Label both. On a multicore machine the second can exceed the first.

**Explore these from the shell first.**

`/proc/cpuinfo` contains one block of `label : value` lines per logical CPU, separated by blank lines:

```bash
head -25 /proc/cpuinfo              # the first CPU's block
grep -c '^processor' /proc/cpuinfo  # number of logical CPUs...
nproc                               # ...compare with this command
grep -m1 'model name' /proc/cpuinfo # the CPU model (x86 machines)
```

Each `processor : N` line starts a new block. On machines with hyperthreading, the number of logical CPUs is larger than the number of physical cores; compare `cpu cores` with `siblings` in one block. The labels depend on the architecture: an x86 laptop shows `model name` and `flags`, while an ARM board such as a Raspberry Pi shows different fields and may have no `model name` line at all. This is why your `cpuinfo` command should display the file as-is rather than look for fixed labels.

`/proc/uptime` is a single line holding two numbers:

```bash
cat /proc/uptime     # e.g. 314.70 1203.55
uptime               # the same information, formatted by a utility
nproc
```

The first number is seconds since boot. The second is the idle time **summed over all CPUs**: if a 4-CPU machine has been up for 100 s and every CPU sat idle the whole time, the second number is about 400. Dividing it by the number of CPUs (`nproc`) and comparing with the first number shows roughly what fraction of time the machine has been idle. (The `uptime` utility also prints load averages; those come from a different file, `/proc/loadavg`, and are not part of this command.) To format the first number as days/hours/minutes/seconds, use integer division: 90061 s is 1 day, 1 hour, 1 minute, 1 second.

## 8. Dynamically updating the terminal

Use two modes to avoid overwriting a command while the user types it:

1. **Live mode:** refresh every two seconds; the only expected input is Enter.
2. **Command mode:** suspend refresh, print `procmon> `, and read a complete command line. Remain in command mode after detail commands. `list` returns to live mode.

For an ANSI-compatible interactive terminal, clear and position the cursor before rendering each live table:

```c
printf("\033[2J\033[H");  /* Clear screen; move cursor to upper left. */
/* Print table and instructions. */
fflush(stdout);
```

The terminal interprets these escape sequences as display instructions rather than printing them literally:

- `\033` is the ESC character.
- `\033[2J` clears the visible screen; it does not itself move the cursor.
- `\033[H` moves the cursor to the upper-left corner.
- `fflush(stdout)` sends buffered output immediately instead of waiting for the buffer to fill.

Each refresh must collect fresh process information and print a complete replacement table. Clearing the screen alone does not update the data. Clearing also removes leftover rows when the new process list is shorter. A carriage return (`\r`) alone only returns to the beginning of the current line; it is insufficient for a multi-line table.

For an initial display-only prototype, redraw followed by `sleep(2)` is acceptable. Replace that sleep with the timed `poll()` described below when adding command input; this allows input to be noticed without waiting for a fixed sleep to finish. Do not use a busy loop that continuously checks for input.

Do not invoke `system("clear")`. Use `isatty(STDIN_FILENO)` and `isatty(STDOUT_FILENO)` to check that both input and output are terminals. For this assignment, it is acceptable to exit with a clear message otherwise. A noninteractive snapshot mode is optional.

A blocking `fgets()` inside the live refresh loop would stop updates while waiting for input. Use `poll()` on standard input with a timeout instead.

**What `poll()` does.** `poll()` asks the kernel: "put me to sleep until at least one of these file descriptors is ready, or until this much time has passed, whichever comes first." While it waits, your process uses **no CPU**: it is in the sleeping (`S`) state, and the kernel wakes it when input arrives or the timer expires. One call therefore covers both things the live mode needs, "refresh every two seconds" and "react when the user presses Enter," without a busy loop and without threads.

You describe what to watch with an array of `struct pollfd`, one element per descriptor. Each element has three fields:

| Field | Set by | Meaning |
| --- | --- | --- |
| `int fd` | you | The descriptor to watch (here `STDIN_FILENO`, which is 0). |
| `short events` | you | The conditions you care about, as bit flags; `POLLIN` means "data is available to read." |
| `short revents` | the kernel | On return, the conditions that actually occurred. This can include `POLLHUP`, `POLLERR`, or `POLLNVAL` even though you never asked for them. |

The call is `poll(array, number_of_elements, timeout_ms)`. A timeout of `-1` waits forever and `0` returns immediately. The return value is the number of elements whose `revents` is nonzero, as listed below the example. Run `man 2 poll` (or `man poll`) for the full definition of `struct pollfd` and every event flag.

```c
#include <poll.h>
#include <unistd.h>

struct pollfd input = { .fd = STDIN_FILENO, .events = POLLIN };
int ready = poll(&input, 1, 2000);  /* Timeout in milliseconds. */
```

- `ready == 0`: the timeout elapsed; refresh the table.
- `ready > 0`: examine `revents`; `POLLIN` means input is ready.
- `ready == -1`: handle errors; retry on `EINTR` as appropriate.
- Handle `POLLHUP`, `POLLERR`, and `POLLNVAL`; do not spin forever on a closed input stream.

In the terminal's normal (canonical) mode, the terminal driver collects typed characters until you press Enter. So `poll()` reports `POLLIN` on standard input only once a **complete line** is available, not after each keystroke. That is exactly the behavior the live mode wants: typing letters does not interrupt the refresh, and pressing Enter does.

Leave the terminal in its normal canonical mode: Enter completes a line. Consume that activation line before printing the command prompt. Use `read()` consistently for terminal input, collect bytes into a bounded line buffer, and preserve any bytes after a newline for the next line. In command mode it is acceptable to block until a command is complete. Detect overlong commands, drain through the newline, and report an error. EOF must exit cleanly.

Do not mix buffered `fgets(stdin)` with descriptor-level polling/read logic: stdio can hold input that `poll()` cannot see. Using stdio for separate `/proc` files is fine.

Recommended control flow:

```text
verify interactive terminal; determine current UID
mode = LIVE
while not quitting:
    if mode is LIVE:
        collect fresh process data and redraw
        poll stdin for up to two seconds
        if activation line arrives:
            consume it; mode = COMMAND
    else:
        print and flush prompt
        read a complete command
        validate and dispatch it
        if command is list: mode = LIVE
        if command is quit or input reaches EOF: stop
release resources
```

### Check your terminal behavior

1. Leave the keyboard untouched: the timestamp and table should refresh approximately every two seconds.
2. Start and finish an example process in another terminal: its row should appear and disappear, without stale rows remaining.
3. Press Enter, then slowly type `status PID`: refresh must stay paused and the text must remain readable.
4. Enter `list`: live refresh should resume.
5. Enter `quit`, or send EOF at the command prompt: the program should exit cleanly.

For long process lists, show a clearly labeled subset that fits the terminal, or implement pagination/scrolling. A clipped live view must indicate that additional processes exist; do not silently imply it is the complete list. Detail commands may use normal terminal scrollback.


## 9. Changing processes and robust behavior

Process directories, threads, and descriptors can disappear between enumeration and reading. Handle failures at each step. Skip unavailable entries during list refresh; use concise messages for explicitly requested views. Do not flood the table with transient error messages.

A series of reads is not one atomic system snapshot. PIDs can also be reused. Recheck ownership on every PID command; do not permanently trust an earlier check. For this educational read-only tool, documenting the remaining race between separate reads is sufficient. For persistent selection or CPU sampling, use PID plus `stat` starttime to detect reuse; more advanced directory-descriptor techniques are optional.

Collect rows before rendering them, then sort. Avoid global exits for a single missing process. Failure to open `/proc` itself should produce a clear fatal error.

## 10. Suggested implementation stages

1. Implement checked reading and parsing for one known PID.
2. Enumerate processes and filter by real UID.
3. Build the sorted process table and timed redraw.
4. Add paused command mode and system-wide commands.
5. Add PID detail commands, directory/link inspection, and error handling.
6. Test changing processes and document limitations.

Keep enumeration, parsing, rendering, and command dispatch in separate functions. You may split them into multiple source files.

Compile with warnings enabled, for example:

```bash
gcc -D_POSIX_C_SOURCE=200809L -std=c11 -Wall -Wextra -Wpedantic \
    procmon.c -o procmon
./procmon
```

Adjust the command if you use several source files. Resolve compiler warnings.

What each option does:

| Option | Meaning |
| --- | --- |
| `-std=c11` | Use the ISO C11 language standard instead of gcc's default GNU dialect. This keeps your code portable, but in this strict mode the system headers hide many POSIX functions that are not part of ISO C, such as `getline()`, `readlink()`, and `strdup()`. Which ones are hidden depends on the header and C library, so do not rely on luck. |
| `-D_POSIX_C_SOURCE=200809L` | `-D` defines a preprocessor macro, as if the file began with `#define _POSIX_C_SOURCE 200809L`. This particular macro is a *feature-test macro*: it tells the system headers to declare the POSIX.1-2008 interfaces (the value `200809L` means "September 2008 edition"). It is what makes the POSIX functions visible again under `-std=c11`. |
| `-Wall` | Enable a large set of common warnings (unused variables, format-string mismatches, missing returns, and so on). Despite the name, it is not *all* warnings. |
| `-Wextra` | Enable additional warnings not covered by `-Wall`, such as unused parameters and signed/unsigned comparisons. |
| `-Wpedantic` | Warn about anything that is not strictly ISO C, such as GNU-specific syntax. `-std=c11` alone still accepts many GNU extensions; this flag is what reports them. |
| `procmon.c` | The source file(s) to compile. |
| `-o procmon` | Name the output executable `procmon` (otherwise gcc names it `a.out`). |

Try removing `-D_POSIX_C_SOURCE=200809L` once: you will see warnings such as `implicit declaration of function 'getline'`. Do not "fix" this by deleting `-std=c11`. Either keep the macro on the command line or put `#define _POSIX_C_SOURCE 200809L` on the first line of each source file, before any `#include`.

Useful additions while developing (optional): `-g` adds debugging information for `gdb`; `-fsanitize=address,undefined` makes the program stop with a precise report on out-of-bounds accesses, use-after-free, and undefined behavior, which is excellent for catching parsing bugs. Remove the sanitizers for your final build if they slow it down. The supplied examples' Makefile additionally uses `-O2` (optimize) and `-pthread` (compile and link with POSIX threads support); your monitor does not create threads, so it does not need `-pthread`.

## 11. Required testing and submission

Test and briefly record results for:

- A known process belonging to your user; compare selected fields with its `/proc` files.
- A process with another real UID, if visible: it must not be listed or inspected.
- `sleep 60 &` started in another terminal: observe appearance and later disappearance.
- An invalid PID, missing PID, malformed PID, unknown command, and extra argument.
- All required commands, including your monitor's own PID.
- A multithreaded process, identifying its PID and TIDs.
- A process that exits during monitoring; keep refreshing without crashing.
- A command line containing multiple arguments and spaces.
- Several minutes of refreshing: the monitor must not steadily accumulate open descriptors or memory from missed cleanup.

### Tests with the supplied workloads (required)

The `proc_lab_examples` folder (Section 14) contains small programs whose behavior is known in advance, so you can check whether your monitor reports it correctly. Read their source code before testing. Each one is short and was written to produce a specific, observable effect in `/proc`.

**Setup.** In terminal A, build and start all four:

```bash
cd proc_lab_examples
make
bash run_all.sh 120     # prints each program's PID; runs for 120 seconds
```

In terminal B, run your monitor. All four programs, and the `bash` process running the launcher, run under your UID and must appear in your live table. Use the PIDs printed in terminal A for the commands below. Exact numbers (RSS, CPU ticks, TIDs, descriptor numbers, addresses) will differ from these examples. Record what *you* observe, and do not hardcode any of these values in your program.

**1. `sleep_threads`: threads and the task list.** Three workers sleep while `main` waits in `pthread_join()`.
- The live table should show `Threads` = 4 and state `S`.
- `tasks PID` should list 4 TIDs, all `S`. One TID equals the PID: that is the main thread.
- Record: explain why there are 4 threads when the program creates 3, and how the TIDs relate to the PID.

**2. `cpu_threads`: a sleeping process whose threads are running.** Two workers compute continuously.
- The live table usually shows the *process* state as `S`, because the state reported for the process is the main thread's, and `main` is blocked in `pthread_join()`.
- `tasks PID` should show the main thread `S` and the two workers `R`.
- Run `stat PID` twice, about 5 seconds apart. `utime` grows quickly while `stime` stays near zero. Convert both readings to seconds. On a machine with two or more CPUs, the process gains about 2 seconds of CPU time per second of wall time, since both workers run at once. On a single CPU it gains at most about 1, because the workers take turns.
- Record: both readings, the growth rate you calculated, and why the process state alone is misleading here. Also compare with `sleep_threads`, whose CPU time barely changes.

**3. `memory_threads`: resident memory, anonymous mappings, a deleted file.** One worker allocates 32 MiB, writes to every byte, and keeps a temporary file open.
- RSS in the live table should be roughly 32 MiB plus the small baseline you see in `sleep_threads` (around 33,000–35,000 KiB). It need not be exactly 32,768 KiB, because RSS also counts the program's code, libraries, and stacks.
- In `maps PID`, look for an unnamed `rw-p` region slightly larger than 32 MiB: a large `malloc()` gets its own anonymous mapping rather than growing `[heap]`. You will likely also see an unnamed region of about 8 MiB (typically the worker thread's stack) and a large region with permissions `---p` (address space reserved but not usable).
- In `fd PID`, one descriptor (often 3) points to a path in `/tmp` ending in `(deleted)`. `tmpfile()` creates a file and immediately removes its name, so the file exists only as long as it stays open.
- Record: the RSS values for `sleep_threads` and `memory_threads`, the `maps` line you believe holds the 32 MiB, and the deleted-file descriptor. Explain why the file still works after being deleted.

**4. `two_files`: descriptors for named files.** `main` opens `first.txt` and `second.txt` with `fopen()` and keeps them open while a worker sleeps.
- `fd PID` should show two targets ending in `/first.txt` and `/second.txt`. Their descriptor numbers must match the `fileno()` values the program printed; under the launcher those values are in `two_files.log` in the log directory printed at startup.
- Record: the full `fd` listing. Explain why the worker thread, which never opened the files, has access to them: all threads in a process share one descriptor table.

**5. Standard streams under the launcher.** In the `fd` output for any of the four programs started by `run_all.sh`, descriptor 0 points to `/dev/null`, and descriptors 1 and 2 point to that program's log file. A shell script's background jobs read from `/dev/null`, and the launcher redirects their output into logs. Now run one program directly in a terminal (for example, `./two_files 60`) and check `fd` again: descriptors 0–2 now point to `/dev/pts/N`. Also inspect the launcher's own `bash` process with `fd`; bash keeps the script it is running open on a high descriptor such as 255.
- Record: both `fd` listings, and what changed and why.

**6. `cmdline` for every workload.** Each should show the program name and the duration argument as separate arguments, for example `./cpu_threads` and `120`.

**7. Processes ending.** When the duration expires (or you press Ctrl+C in terminal A), all four programs exit. Their rows must disappear from the live table within a refresh or two, and running a PID command on one of them must report that the process is no longer available. Your monitor must keep running.

**8. Changing observations.** Run `tasks PID` and watch the live table during the first second or two of a run, and again as the programs finish. The thread counts can briefly be lower than the steady-state values while threads are still being created or have already exited. Note also what does *not* change: the `cpu_threads` workers show `R` the whole time, because `R` means running **or runnable**, so a thread that is ready but waiting for a CPU still shows `R`. The `sleep_threads` workers wake for only a moment each second, so you will almost always catch them in `S`. Record what you see, and explain why two reads taken a moment apart can disagree (see Section 9).

Do not require exact agreement between tools sampled at different times. Document any permission-denied cases you encounter; do not change system permissions to make them pass.

Submit source files, a Makefile or exact build command, a README with interaction instructions, and a short test report with representative output. Also answer the following (2–4 sentences each):

1. Why can a readable `/proc` file report a file size of zero?
2. How does a `FILE *` differ from an integer file descriptor?
3. Why can a process disappear after `readdir()` succeeds?
4. Why is `cmdline` different from an ordinary line-oriented text file?
5. How do process PID, thread TID, and thread-group ID relate?
6. Why is accumulated CPU time not CPU utilization percentage?
7. Why should the monitor inspect descriptor links instead of reading their targets?

## 12. Grading rubric

| Component | Points |
| --- | ---: |
| Correct enumeration and current-user filtering, including PID commands | 15 |
| Correct live table, fields, units, and numeric sorting | 15 |
| Dynamic refresh and usable paused command mode | 15 |
| Six PID detail commands: status, stat, cmdline, maps, fd, tasks | 25 |
| cpuinfo, meminfo, uptime, help, list, and quit | 10 |
| Input validation, cleanup, and handling changing/inaccessible entries | 10 |
| Build instructions, test evidence, and conceptual answers | 10 |
| **Total** | **100** |

Optional extensions (not required for full credit): filtering by name, sorting by memory, pagination, adjustable refresh interval, or interval CPU utilization. For CPU utilization, sample the change in `utime + stime` over monotonic elapsed time and divide by clock ticks per second; state whether 100% represents one core or the whole machine. Detect PID reuse using starttime.

## 13. References

Consult the Linux manual pages and kernel documentation; local page availability varies by distribution.

- [Linux kernel: /proc filesystem](https://www.kernel.org/doc/html/latest/filesystems/proc.html)
- [proc_pid_status(5)](https://man7.org/linux/man-pages/man5/proc_pid_status.5.html)
- [proc_pid_stat(5)](https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html)
- [proc_pid_cmdline(5)](https://man7.org/linux/man-pages/man5/proc_pid_cmdline.5.html)
- [proc_pid_maps(5)](https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html)
- [proc_pid_fd(5)](https://man7.org/linux/man-pages/man5/proc_pid_fd.5.html)
- [proc_pid_task(5)](https://man7.org/linux/man-pages/man5/proc_pid_task.5.html)
- [readdir(3)](https://man7.org/linux/man-pages/man3/readdir.3.html)
- [readlink(2)](https://man7.org/linux/man-pages/man2/readlink.2.html)
- [poll(2)](https://man7.org/linux/man-pages/man2/poll.2.html)

Useful local commands include `man 3 fopen`, `man 3 getline`, `man 2 open`, `man 2 read`, `man 3 readdir`, and `man 2 poll`.


## 14. Supplied pthread workloads

Use the accompanying `proc_lab_examples` folder to test your monitor. It includes four small C applications, a shared helper header, a Makefile, and one launcher. Build with `make` and run `bash run_all.sh 120` inside that folder in a separate terminal (Section 11 explains the full setup). Run your monitor under the same user in another terminal.

| Application | Behavior | Steady-state thread count, including main |
| --- | --- | ---: |
| `sleep_threads` | Three sleeping worker threads | 4 |
| `cpu_threads` | Two computing worker threads | 3 |
| `memory_threads` | One worker touches 32 MiB and holds a temporary file open | 2 |
| `two_files` | Opens first.txt and second.txt while a worker sleeps | 2 |

The launcher prints PIDs, redirects application output to individual logs, and starts the applications concurrently. They finish naturally after the supplied duration; Ctrl+C stops the remaining children. The README describes the build steps and expected behavior. The tests to perform with these programs, and what to record, are listed under "Tests with the supplied workloads" in Section 11. They are part of the required test report and are graded in its test-evidence row; they do not change the 100-point total.

Check `status`, `stat`, `cmdline`, `maps`, `fd`, and `tasks` using these PIDs. A process's main thread may be sleeping in `pthread_join()` while worker threads consume CPU. RSS and state observations vary with timing. The memory allocation need not appear under `[heap]`, and the temporary file may have a deleted/unnamed target. Do not hardcode file descriptor numbers or exact memory readings.


### Inspecting two named files

The supplied `two_files.c` opens `first.txt` and `second.txt` using `fopen(..., "r")`. Both streams remain open while main joins a sleeping worker. Run `make` and then `./two_files 60` from the examples directory, or use the launcher to run it alongside the other examples.

Use the printed PID with your monitor's `fd PID` command. Identify both filenames and compare their descriptor numbers to the `fileno()` values printed by the example. Descriptors 3 and 4 are common but not guaranteed. There will normally also be descriptors for standard input, output, and error; two opened text files does not imply only two total descriptors. What to record for this program is listed in Section 11, test 4.
