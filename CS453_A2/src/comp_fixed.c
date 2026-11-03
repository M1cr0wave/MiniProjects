#include <seccomp.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    /*** Policy: app1 may make any syscall it needs, but must NOT open files ***/
    scmp_filter_ctx ctx;
    ctx = seccomp_init(SCMP_ACT_ALLOW);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(openat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(openat2), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(creat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open_by_handle_at), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open_tree), 0);
    /* io_uring can perform IORING_OP_OPENAT without a seccomp-visible syscall */
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_setup), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_enter), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(io_uring_register), 0);
    seccomp_load(ctx);

    /// launch app1 inside the compartment
    char *args[] = {"./app1", NULL};
    execve(args[0], args, NULL);
    return 1;
}
