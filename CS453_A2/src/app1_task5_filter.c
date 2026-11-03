#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <seccomp.h>
#include <sys/types.h>

#define NAME_SIZE 16

int main(int argc, char **argv)
{
    scmp_filter_ctx ctx;
    ctx = seccomp_init(SCMP_ACT_KILL);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
    seccomp_load(ctx);

    FILE *in = fopen("input.txt", "r");
    if (in == NULL) { perror("fopen input.txt"); return 1; }
    FILE *out = fopen("output.txt", "w");
    if (out == NULL) { perror("fopen output.txt"); return 1; }

    char name[NAME_SIZE] = {0};
    if (fgets(name, sizeof(name), in) == NULL) { perror("fgets"); return 1; }
    name[strcspn(name, "\n")] = '\0';

    printf("Input file contents:\n %s\n", name);

    fprintf(out, "Hello, ");
    fprintf(out, "%s", name);
    fprintf(out, "!\n");

    fclose(in);
    fclose(out);
    return 0;
}
