#include <fcntl.h>
#include <unistd.h>
#include <seccomp.h>
#include <stdio.h>

/* KILL a file-opening syscall if its flags argument (arg index `a`) requests
   any write access, creation or truncation. */
static void deny_write_flags(scmp_filter_ctx ctx, int sc, unsigned int a)
{
    /* access mode = flags & O_ACCMODE: allow only O_RDONLY (0) */
    seccomp_rule_add(ctx, SCMP_ACT_KILL, sc, 1,
                     SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_ACCMODE, O_WRONLY));
    seccomp_rule_add(ctx, SCMP_ACT_KILL, sc, 1,
                     SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_ACCMODE, O_RDWR));
    seccomp_rule_add(ctx, SCMP_ACT_KILL, sc, 1,
                     SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_ACCMODE, O_ACCMODE));
    /* O_RDONLY|O_CREAT would create a file, O_RDONLY|O_TRUNC truncates one */
    seccomp_rule_add(ctx, SCMP_ACT_KILL, sc, 1,
                     SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_CREAT, O_CREAT));
    seccomp_rule_add(ctx, SCMP_ACT_KILL, sc, 1,
                     SCMP_CMP(a, SCMP_CMP_MASKED_EQ, O_TRUNC, O_TRUNC));
}

int main(int argc, char **argv)
{
    /*** 1. Open the output files BEFORE the filter exists, on fds 3, 4, 5 ***/
    const char *paths[3] = {"output0.txt", "output1.txt", "output2.txt"};
    for (int k = 0; k < 3; k++) {
        int fd = open(paths[k], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { perror(paths[k]); return 1; }
        if (fd != 3 + k) {
            if (dup2(fd, 3 + k) < 0) { perror("dup2"); return 1; }
            close(fd);
        }
    }

    /*** 2. Build and load the filter ***/
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    /* default bad-arch action is KILL: 32-bit int 0x80 / x32 syscalls are killed */

    /* (i) audit writes to fd 4 (rank 1's output file) */
    seccomp_rule_add(ctx, SCMP_ACT_LOG, SCMP_SYS(write), 1,
                     SCMP_A0(SCMP_CMP_EQ, 4));

    /* (ii) read-only opens allowed, any write/create/truncate open killed */
    deny_write_flags(ctx, SCMP_SYS(openat), 2);   /* openat(dirfd, path, flags, mode) */
    deny_write_flags(ctx, SCMP_SYS(open), 1);     /* open(path, flags, mode) */

    /* (iii) Task 7 loopholes: other ways to open/create files */
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(creat), 0);  /* always O_WRONLY|O_CREAT|O_TRUNC */
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(openat2), 0);/* flags live in a struct pointer: uninspectable */
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open_by_handle_at), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open_tree), 0);
    /* io_uring can issue IORING_OP_OPENAT without any syscall seccomp sees */
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_setup), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_enter), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_register), 0);

    if (seccomp_load(ctx) != 0) { fprintf(stderr, "seccomp_load failed\n"); return 1; }
    seccomp_release(ctx);

    /*** 3. Launch app3 inside the compartment (no fds passed via argv) ***/
    char *args[] = {"./app3", NULL};
    execve(args[0], args, NULL);
    perror("execve ./app3");
    return 1;
}
