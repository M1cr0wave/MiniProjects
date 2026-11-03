#include <stdio.h>
#include <string.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>

#define NAME_SIZE 16

int main(int argc, char **argv)
{
    FILE *in = fopen("input.txt", "r");
    if (in == NULL) { perror("fopen input.txt"); return 1; }
    FILE *out = fopen("output.txt", "w");
    if (out == NULL) { perror("fopen output.txt"); return 1; }

    prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);

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
