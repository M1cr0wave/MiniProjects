#include <fcntl.h>
#include <unistd.h>
#include <seccomp.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    /*** Setup seccomp in logging mode for auditing ***/
    scmp_filter_ctx ctx;
    ctx = seccomp_init(SCMP_ACT_ALLOW);
    seccomp_rule_add(ctx, SCMP_ACT_LOG, SCMP_SYS(write), 1, SCMP_A0(SCMP_CMP_EQ, 4));
    seccomp_load(ctx);

    const char *paths[3] = {"output0.txt", "output1.txt", "output2.txt"};
    for (int k = 0; k < 3; k++) {
        int fd = open(paths[k], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0)
            close(fd);
    }

    if (argc != 2)
        printf("!!!! Need ID number as second argument\n");
    else {
        char *args[] = {"./app2", argv[1], NULL};
        execve(args[0], args, NULL);
        perror("execve ./app2");
    }
}
