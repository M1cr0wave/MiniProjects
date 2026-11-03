#include <seccomp.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    scmp_filter_ctx ctx;
    ctx = seccomp_init(SCMP_ACT_ALLOW);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open), 0);
    seccomp_load(ctx);

    char *args[] = {"./app1", NULL};
    execve(args[0], args, NULL);
    return 1;
}
