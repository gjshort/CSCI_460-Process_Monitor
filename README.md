# Process Monitor (procmon)

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