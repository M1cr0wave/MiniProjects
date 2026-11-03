# CS 453/653 Assignment 2: Compartmentalization with seccomp

> **Completed by model: `claude-opus-5-5` (Claude Opus 5.5, Anthropic), running in Claude Code.**
> (The session metadata reports both the configured model and the last-served model as `claude-opus-5-5`.)

**Name / Student ID:** `<fill in>`

Everything below was actually compiled and run on **Ubuntu 24.04 (x86_64, glibc 2.39, libseccomp 2.5)**.
The traces quoted here are real output. The screenshots the assignment asks for are not included. To make them,
rerun the commands from `src/` on your own machine and capture the terminal. PIDs and addresses will
differ from the ones shown. On Ubuntu 22.04, `fstat()` may show up as `newfstatat` in the trace. If it does, use that name in the filter.

---

## Task 1 (0.5 pt): app1

```
$ cat input.txt        # (a)  -> <YourFirstName>
$ cat output.txt       # (b)  -> (empty)
$ gcc -o app1 app1.c && ./app1   # (c)
Input file contents:
 <YourFirstName>
$ cat output.txt       # (d)  -> Hello, <YourFirstName>!
```

---

## Task 2 (1 pt): `strace ./app1` (run in a real terminal)

```
execve("./app1", ["./app1"], 0x7ffe... /* vars */) = 0                     <-- (a) runs the executable
... dynamic loader / libc startup (brk, mmap, openat ld.so.cache, libc.so.6, mprotect, ...) ...
openat(AT_FDCWD, "input.txt", O_RDONLY) = 3                                <-- (b)
openat(AT_FDCWD, "output.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 4         <-- (b)
fstat(3, {st_mode=S_IFREG|0644, st_size=5, ...}) = 0
read(3, "John\n", 4096)                 = 5                                <-- (c) read input.txt
fstat(1, {st_mode=S_IFCHR|0600, ...}) = 0
write(1, "Input file contents:\n", 21)  = 21                               <-- (c) terminal
write(1, " John\n", 6)                  = 6                                <-- (c) terminal
fstat(4, {st_mode=S_IFREG|0644, st_size=0, ...}) = 0
close(3)                                = 0
write(4, "Hello, John!\n", 13)          = 13                               <-- (c) output.txt
close(4)                                = 0
exit_group(0)                           = ?
```

- **(a)** `execve("./app1", ...)` is the first line of the trace. The shell has already `fork`ed, and `execve` replaces that child with `./app1`.
- **(b)** The two `openat(AT_FDCWD, "input.txt", O_RDONLY) = 3` and `openat(AT_FDCWD, "output.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 4` lines.
- **(c)** `read(3, ...)` reads input.txt. `write(1, ...)` writes to the terminal. `write(4, ...)` writes to output.txt.
- **How strace tells the writes apart:** by the **first argument, the file descriptor**. `openat` returned **3** for input.txt and **4** for output.txt. FD **1** is stdout (the terminal), which the process inherited without opening it. So `write(1, …)` goes to the terminal and `write(4, …)` goes to output.txt.
- **(d)** **`openat`** appears instead of `open`. `fopen()` is a libc function, not a syscall. glibc implements it, and also its own `open()` wrapper, with the `openat(AT_FDCWD, path, flags, mode)` syscall. `AT_FDCWD` means "relative to the current directory," which makes it equivalent to `open`. Modern glibc on x86_64 never issues the legacy `open` syscall (number 2). It always uses `openat` (number 257). strace shows the syscall the kernel actually received, not the library call in the source. `fopen(..., "w")` also becomes the flags `O_WRONLY|O_CREAT|O_TRUNC` with mode 0666.

---

## Task 3 (1 pt): buffering

- **(a)** stdio keeps a user-space buffer for each `FILE*`, usually 4096 bytes for a regular file. A regular file is **fully buffered**, so the three `fprintf()` calls just copy `"Hello, "`, `"John"` and `"!\n"` into that buffer. Nothing reaches the kernel until the buffer fills or is flushed. The line that actually triggers the single `write(4, "Hello, John!\n", 13)` is **`fclose(out);`**, because `fclose` flushes the buffer and then calls `close(4)`.
- **(b)** Look at the FD argument. `write(1, …)` goes to the terminal (stdout) and `write(4, …)` goes to output.txt. The FD numbers come from the return values of the `openat` calls. You can also tell from the content and position: the terminal writes contain `"Input file contents:"`, and the output.txt write comes right before `close(4)`.
- **(c)** `strace -o trace.txt ./app1 > terminal.log` gives:
  ```
  read(3, "John\n", 4096)                 = 5
  fstat(1, {st_mode=S_IFREG|0644, st_size=0, ...}) = 0     <-- stdout is now a regular file
  fstat(4, ...) = 0
  close(3)                                = 0
  write(4, "Hello, John!\n", 13)          = 13
  close(4)                                = 0
  write(1, "Input file contents:\n John\n", 27) = 27      <-- ONE write, at the very end
  exit_group(0)                           = ?
  ```
  In Task 2 there were **two** `write(1, …)` calls, and they came **right after the `read`, before** the write to output.txt. Now there is **one** `write(1, …)` of 27 bytes, and it comes **last, after `close(4)` and just before `exit_group`**.
  **Why:** stdio picks stdout's buffering mode on the first output, using the `fstat(1)` you can see in the trace. If stdout is a terminal (`S_IFCHR`, i.e. a tty) it is **line-buffered**, so each `\n` flushes, and the two newlines in the `printf` give two writes. If stdout is redirected to a file (`S_IFREG`) it is **fully buffered**, so the whole `printf` output stays in the buffer. The program never calls `fclose(stdout)`, so the buffer is only flushed when `return 0` → `exit()` runs the stdio cleanup. That happens after `fclose(out)` has already written output.txt.

---

## Task 4 (1 pt): strict mode after the `fopen`s

Result: the process is killed (`Killed`, exit status 137 = 128 + SIGKILL).
```
openat(AT_FDCWD, "input.txt", O_RDONLY) = 3
openat(AT_FDCWD, "output.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 4
prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) = 0
fstat(3,  <unfinished ...>)             = ?
+++ killed by SIGKILL +++
```
- **(a)** **`fstat`** on fd 3 (`newfstatat` on some glibc/Ubuntu 22.04 builds) caused the termination.
- **(b)** `fgets()` is the first stdio operation on the `in` stream, and stdio allocates the stream's buffer lazily. To choose a buffer size it calls `fstat()` on the descriptor to read `st_blksize`, and it also checks whether the FD is a tty. glibc makes this call internally (`_IO_file_doallocate`), so it never appears in app1.c. The same thing happens for stdout (`fstat(1)`) and for `out` (`fstat(4)`) on their first use.
- **(c)** Strict mode allows only **`read`, `write`, `_exit`/`exit`, and `sigreturn`**. Any other syscall, including `fstat`, `close`, `brk`, `mmap` and `exit_group`, gets SIGKILL. The design idea is that a strict-mode process may only work on descriptors it already holds. It may not inspect files, allocate memory or do anything else, so `fstat` is not on the list.
- **(d)** If `prctl` runs at the very start of `main()`, the killing syscall is **`getrandom`**:
  ```
  munmap(0x7f3c5586e000, 44439)           = 0
  prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) = 0
  getrandom( <unfinished ...>)            = ?
  +++ killed by SIGKILL +++
  ```
  `fopen()` calls `malloc()` to allocate the `FILE` structure. The first `malloc` initializes the tcache, and glibc ≥ 2.34 seeds the tcache key with `getrandom(..., GRND_NONBLOCK)`. The `brk` calls to grow the heap come next, but the process never gets that far. It never reaches `openat` either.

---

## Task 5 (1 pt): filter mode

Baseline filter (read/write/exit): killed with `Bad system call` (SIGSYS, exit 159), and strace shows `openat(AT_FDCWD, "input.txt", O_RDONLY) = 257` then `+++ killed by SIGSYS +++`.
Note: `SCMP_ACT_KILL` means *kill thread* and delivers SIGSYS, while strict mode delivers SIGKILL.

**(a) Final filter** (`src/app1_task5_filter.c`):
```c
scmp_filter_ctx ctx;
ctx = seccomp_init(SCMP_ACT_KILL);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);   // newfstatat on 22.04 if that's what strace shows
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
seccomp_load(ctx);
```
`exit` from the template is **not** needed. `return 0` from main calls `exit()`, which issues **`exit_group`**, not `exit`, so I removed it.

**(b)** `./app1` prints `Input file contents: / John`, exits 0, and output.txt contains `Hello, John!`.

**(c) Method:** I ran `strace -f ./app1` and looked only at the syscalls **after** the `seccomp(SECCOMP_SET_MODE_FILTER, …) = 0` line. Everything before it (`brk`, `getrandom`, `prctl(PR_SET_NO_NEW_PRIVS)` and libseccomp's probing `seccomp(...)` calls) runs before the filter is active, so it doesn't need a rule. That left `openat, fstat, read, write, close, exit_group`. Each run against the filter dies at the first missing syscall, and strace's last line before `killed by SIGSYS` names it. I then checked minimality by removing each rule in turn. Every removal caused SIGSYS.

**(d) Default `SCMP_ACT_ERRNO(EPERM)`, no `openat` rule:**
```
$ ./app1
fopen input.txt: Operation not permitted
$ strace ./app1
seccomp(SECCOMP_SET_MODE_FILTER, 0, {len=13, ...}) = 0
openat(AT_FDCWD, "input.txt", O_RDONLY) = -1 EPERM (Operation not permitted)
dup(2)                                  = -1 EPERM (Operation not permitted)
write(2, "fopen input.txt: Operation not p"..., 41) = 41
exit_group(1)                           = ?
+++ exited with 1 +++
```
- (i) It printed `fopen input.txt: Operation not permitted`, generated by `if (in == NULL) { perror("fopen input.txt"); return 1; }`, the error check after the **first** `fopen`. The program then returned 1, so the second `fopen` was never reached.
- (ii) The filter did not run the syscall. It returned **`-1` with errno `EPERM`** (`= -1 EPERM`). The glibc syscall wrapper stores the negated kernel return value in `errno` (`errno = EPERM`, value 1), and `fopen()` returns `NULL`. `perror(s)` prints `s`, then `": "`, then `strerror(errno)`, which is `"Operation not permitted"` for EPERM. The `dup(2) = -1 EPERM` line is perror's internal attempt to duplicate stderr. It is also denied by the default action, so perror falls back to writing to fd 2 directly.

I then restored the `SCMP_ACT_KILL` version.

---

## Task 6 (1 pt): comp1

**(a) Filter** (`src/comp1.c`):
```c
ctx = seccomp_init(SCMP_ACT_KILL);
/* app1 functionality (Task 5) */
read, write, exit_group, openat, fstat, close
/* needed for execve() + loading the new program */
execve, brk, mmap, access, pread64, arch_prctl, set_tid_address,
set_robust_list, rseq, mprotect, prlimit64, munmap, getrandom
seccomp_load(ctx);
execve("./app1", args, NULL);
```
(each name is a `seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(name), 0);`)

**(b)** `./comp1` prints `Input file contents: / John`, exit status 0, and output.txt = `Hello, John!`. With only the Task 5 set, it dies immediately: `execve("./app1", ["./app1"], NULL) = 59` then `killed by SIGSYS` (59 is the number of `execve`). I verified that removing any one of the 13 extra syscalls makes it fail.

**(c) Why extra syscalls:** a seccomp filter **persists across `execve`** and can never be removed. It now applies to everything from the `execve` call itself onward. That includes the dynamic loader (`ld-linux`) and libc start-up code that run *before* app1's `main()`:
- `execve` is the launch itself.
- `brk`, `mmap`, `munmap` and `mprotect` set up the heap, map `ld.so.cache` and `libc.so.6`, and set RELRO/read-only segments.
- `access("/etc/ld.so.preload")` is the loader's preload check. `pread64` reads the ELF program headers of libc.
- `arch_prctl(ARCH_SET_FS)` sets up thread-local storage. `set_tid_address`, `set_robust_list` and `rseq` handle thread initialization.
- `prlimit64(RLIMIT_STACK)` queries the stack limit. `getrandom` is the malloc tcache key/stack protector randomness.

**(d) Why not needed in Task 5:** in Task 5 the filter was loaded **inside `main()`** of an already-running app1. All the loader and libc initialization (exec, mmaps, TLS, `getrandom`/`brk` for malloc, which ran inside `seccomp_init`) had already happened while the process was still unrestricted. Only the syscalls made *after* `seccomp_load` had to be allowed.

**(e) Allow `input.txt` but not `output.txt`? No, not reliably.** The filename argument of `openat` is a **pointer** (`const char *`) into the process's user memory. A classic seccomp-BPF filter only sees the raw 64-bit register values in `struct seccomp_data`. It can compare the pointer value, but it cannot dereference it to read the string. Even if it could, there would be a TOCTOU race: another thread could change the string after the check and before the kernel reads it. Path variants (`./output.txt`, `../dir/output.txt`, symlinks, hard links, a different `dirfd`) also break any name-based check. seccomp *can* filter on the integer flags argument, for example to deny write modes (see Task 9), but path-based policy needs other tools: Landlock, AppArmor/SELinux, a mount namespace/chroot, or `SECCOMP_RET_USER_NOTIF` with a supervisor.

---

## Task 7 (1.5 pt): broken compartment

**(a)** With input.txt = `Bob`, `./comp_broken` prints `Input file contents: / Bob` and output.txt becomes `Hello, Bob!`. So app1 still opened, read and wrote files.

**(b)** `strace -f ./comp_broken`:
```
seccomp(SECCOMP_SET_MODE_FILTER, 0, {len=8, ...}) = 0      <-- filter only kills syscall 2 (open)
execve("./app1", ["./app1"], NULL)      = 0
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
openat(AT_FDCWD, "input.txt", O_RDONLY) = 3                <-- opened!
openat(AT_FDCWD, "output.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 4   <-- opened!
```
seccomp matches on the **syscall number**, not on the high-level intent. `SCMP_SYS(open)` is x86_64 syscall **2**. glibc's `fopen()` and `open()`, and the dynamic loader, all use **`openat`**, syscall **257**. That is a different number, so it falls through to the default `SCMP_ACT_ALLOW`. The rule never matches anything in app1. The policy is a denylist, and it misses the other ways of opening a file.

**(c) Fix** (`src/comp_fixed.c`). These are the x86_64 syscalls that open or create a file descriptor for a path (from `asm/unistd_64.h`):

| syscall | nr | why |
|---|---|---|
| `open` | 2 | legacy open |
| `creat` | 85 | `open(O_CREAT|O_WRONLY|O_TRUNC)` |
| `openat` | 257 | what glibc actually uses |
| `open_by_handle_at` | 304 | opens a file from a `name_to_handle_at` handle |
| `open_tree` | 428 | returns an fd for a path/mount (O_PATH-like) |
| `openat2` | 437 | newer openat with `struct open_how` |
| `io_uring_setup/enter/register` | 425/426/427 | `IORING_OP_OPENAT/OPENAT2` opens files **inside the kernel without any syscall seccomp can see**, so io_uring must be denied too |

```c
ctx = seccomp_init(SCMP_ACT_ALLOW);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(openat), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(openat2), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(creat), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open_by_handle_at), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open_tree), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_setup), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_enter), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_register), 0);
seccomp_load(ctx);
```
Result: `Bad system call`. strace shows `openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 257` → `killed by SIGSYS`. app1 can't open any file, not even its shared libraries. 32-bit (`int 0x80`, i386 numbering) and x32 syscalls are another bypass route: on i386, `open` is number 5. libseccomp's default *bad-arch* action is KILL, so syscalls from a non-native ABI are killed automatically. This denylist approach is still fragile in general. An allowlist (default KILL, as in Task 6) is the safer design.

---

## Task 8 (1 pt): logging mode

Worked example with `12345678`; **redo the table with your own ID**.

**(a) Prediction.** In comp2, each `open(...)` is immediately followed by `close`, so all three get fd 3 and nothing is left open across `execve`. In app2, fds 0–2 are stdin/stdout/stderr, and `open` returns the **lowest free descriptor**. The three `openat`s therefore return **3, 4, 5** in order (confirmed in strace: `openat(AT_FDCWD,"output0.txt",O_WRONLY) = 3`, `output1 = 4`, `output2 = 5`). This is the same rule seen in Task 2, where the first opened file got 3 and the second got 4. So **rank 0 → fd 3, rank 1 → fd 4, rank 2 → fd 5**. Rank *r* calls `write()` once for each of the first 8 digits *d* with *d* mod 3 = *r*:

| rank | digits (mod 3 = r) | # writes | fd |
|---|---|---|---|
| 0 | {0,3,6,9} → 3, 6 | 2 | 3 |
| 1 | {1,4,7} → 1, 4, 7 | 3 | 4 |
| 2 | {2,5,8} → 2, 5, 8 | 3 | 5 |

**(b) Filter:**
```c
ctx = seccomp_init(SCMP_ACT_ALLOW);
seccomp_rule_add(ctx, SCMP_ACT_LOG, SCMP_SYS(write), 1, SCMP_A0(SCMP_CMP_EQ, 4));
seccomp_load(ctx);
```
`journalctl -k -f --since "now"` (or `dmesg`) showed **exactly 3** entries, which matches the prediction:
```
audit: type=1326 ... pid=825 comm="app2" exe=".../app2" sig=0 arch=c000003e syscall=1 compat=0 ip=0x7f97efd1c844 code=0x7ffc0000
audit: type=1326 ... pid=825 comm="app2" ... syscall=1 ...
audit: type=1326 ... pid=825 comm="app2" ... syscall=1 ...
```
`type=1326` is AUDIT_SECCOMP, `syscall=1` is `write` on x86_64, `code=0x7ffc0000` is SECCOMP_RET_LOG, and `arch=c000003e` is x86_64.

**(c)** The logged `pid=825` matches `(rank=1 pid=825)` in output1.txt, so the **rank 1 worker** made the logged writes. In the same run, output0.txt shows `rank=0 pid=824`. 824 is the PID of the original process: `execve` does **not** create a process. It replaces the program image and keeps the PID, so comp2 and the rank 0 app2 have the same PID. The rank 1 process was created by `fork()`, which makes a **new process with a new PID** (825). Rank 2 is a second fork (826).

**(d)** (i) `strace -f ./comp2 <ID>` shows that `fork()` is implemented by the **`clone`** syscall:
```
clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f6a7ba55a10) = 852
[pid 851] clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD <unfinished ...>
[pid 851] <... clone resumed>, child_tidptr=0x7f6a7ba55a10) = 853
[pid 852] write(4, "(rank=1 pid=852) (i=0) value = 1"..., 33 ...
```
glibc's `fork()` wrapper issues `clone` with `SIGCHLD` and no `CLONE_VM` flag, not the legacy `fork` syscall (57).
(ii) **No.** The logging process did not exist when the filter was installed. comp2 loaded the filter, then `execve`d app2, and only then did app2 `clone` the rank 1 child. The child's writes were still logged. So **seccomp filters are inherited by child processes across `fork`/`clone`, and preserved across `execve`**. A filter applies to the whole process tree created after it, and a child cannot remove or loosen it. It can only add stricter filters. This is why a compartment launcher can safely sandbox a program that spawns workers.

---

## Task 9 (2 pt): putting it all together

Source: `src/comp3.c`, `src/app3.c`.

**(a) comp3 filter** (default ALLOW, because this is an audit-style compartment):
```c
/* (i) log writes to fd 4 */
seccomp_rule_add(ctx, SCMP_ACT_LOG, SCMP_SYS(write), 1, SCMP_A0(SCMP_CMP_EQ, 4));

/* (ii) for openat (flags = arg 2) and open (flags = arg 1): KILL if
   (flags & O_ACCMODE) is O_WRONLY, O_RDWR or 3, or if O_CREAT / O_TRUNC is set */
SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_ACCMODE, O_WRONLY)
SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_ACCMODE, O_RDWR)
SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_ACCMODE, O_ACCMODE)
SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_CREAT,   O_CREAT)
SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_TRUNC,   O_TRUNC)

/* (iii) Task 7 loopholes */
KILL creat, openat2 (flags hidden in a struct pointer, can't be inspected),
     open_by_handle_at, open_tree, io_uring_setup/enter/register
/* non-native ABIs (i386 int 0x80, x32) -> killed by libseccomp's default bad-arch action */
```
Separate rules for the same syscall are OR'ed, so any one matching condition kills. `O_RDONLY` is 0, so a test like "flags & X == 0" can't single out read-only on its own. Instead the filter **masks the access-mode bits** (`O_ACCMODE` = 3) and kills every non-zero mode. I also block `O_CREAT` and `O_TRUNC` because on Linux `O_RDONLY|O_TRUNC` really truncates a file and `O_RDONLY|O_CREAT` creates one. Both would modify the filesystem even though the mode is "read-only."
**Demonstration:** building app3 with `-DTRY_WRITE_OPEN` makes it call `open("output0.txt", O_WRONLY)` first. `./comp3` prints `Bad system call` (exit 159), and strace shows `openat(AT_FDCWD, "output0.txt", O_WRONLY) = 257` then `+++ killed by SIGSYS +++`. I also ran a probe program under the same filter: `O_RDONLY` was allowed, while raw `open(O_WRONLY|O_CREAT)`, `O_RDWR`, `creat`, `O_RDONLY|O_CREAT`, `O_RDONLY|O_TRUNC`, `openat2` and `io_uring_setup` were all killed.

**(b)** comp3 opens `output0/1/2.txt` with `O_WRONLY|O_CREAT|O_TRUNC` **before** creating the filter. It places them on **3, 4, 5** with `dup2(fd, 3+k)`, closing the original if it differs. Then it loads the filter and `execve("./app3", {"./app3", NULL}, NULL)`. app3 just uses `int fds[3] = {3, 4, 5};` and never opens an output file. No FD numbers are passed in argv.

**(c)** app3 calls `open("id.txt", O_RDONLY)`, reads up to 8 bytes, keeps the leading digits (which drops the newline), and closes the file. It then forks and writes as before. `./comp3` with `id.txt = 12345678` produced the same three files as Task 8, and `dmesg`/journalctl showed **3** `syscall=1` entries from `pid=880`, which is the rank 1 process (`(rank=1 pid=880)` in output1.txt).

**(d)**
- (i) seccomp checks permissions **when a syscall is made**, not on objects the process already holds. The files were opened for writing by comp3 *before* any filter existed. Open file descriptions without `O_CLOEXEC` survive `execve`, so app3 inherits fds 3–5 already open in write mode. A `write(3/4/5, …)` call is allowed, because the only `write` rule is LOG for fd 4. Write access was granted at open time, and the filter only restricts *new* opens.
- (ii) app3 opens id.txt with `openat(AT_FDCWD, "id.txt", O_RDONLY)`. Its access mode is 0 and it has no `O_CREAT`/`O_TRUNC`, so none of the masked-compare KILL rules match, and it falls through to the default `ALLOW`. The dynamic loader's `O_RDONLY|O_CLOEXEC` opens of libc pass for the same reason.
- (iii) A seccomp filter applies to the process that loads it, and to all its future children and exec'd images. It **cannot be removed or relaxed** afterwards. If comp3 loaded the filter first, its own `open(..., O_WRONLY|O_CREAT|O_TRUNC)` of the output files would be killed with SIGSYS, just like app3's. So comp3 must do every privileged action (opening the files for writing) first, then drop the privilege by loading the filter, and only then run the untrusted code. This is the privilege-separation pattern: acquire resources, then lock down, then exec.

**(e)** Screenshots to capture: `src/app3.c`, `src/comp3.c`, `cat id.txt`, `cat output0.txt output1.txt output2.txt`, the `./comp3` terminal, the `journalctl -k -f` window showing the fd-4 entries, and the `TRY_WRITE_OPEN` SIGSYS run.

---

### Build / run cheat-sheet
```
sudo apt-get install strace libseccomp-dev
gcc -o app1 app1.c
gcc -o comp1 comp1.c -lseccomp
gcc -o comp_broken comp_broken.c -lseccomp ; gcc -o comp_fixed comp_fixed.c -lseccomp
gcc -o app2 app2.c ; gcc -o comp2 comp2.c -lseccomp ; ./comp2 <ID>
gcc -o app3 app3.c ; gcc -o comp3 comp3.c -lseccomp ; echo <ID> > id.txt ; ./comp3
journalctl -k -f --since "now"      # in a second terminal (or: sudo dmesg -w)
```
