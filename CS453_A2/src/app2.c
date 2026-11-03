#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#define ID_SIZE 8
#define BUF_SIZE 100
#define NUM_WORKERS 3

int main(int argc, char **argv)
{
    char buf[BUF_SIZE];
    int fds[NUM_WORKERS];
    const char *paths[NUM_WORKERS] = {"output0.txt", "output1.txt", "output2.txt"};

    for (int k = 0; k < NUM_WORKERS; k++)
        fds[k] = open(paths[k], O_WRONLY);

    char id[ID_SIZE];
    int upper_limit = ID_SIZE;
    if (strlen(argv[1]) < ID_SIZE)
        upper_limit = (int)strlen(argv[1]);
    memcpy(id, argv[1], upper_limit);

    int rank = 0;
    if (fork() == 0)
        rank = 1;
    else if (fork() == 0)
        rank = 2;

    for (int i = 0; i < upper_limit; i++) {
        int digit = id[i] - 0x30;
        if (digit % 3 == rank) {
            sprintf(buf, "(rank=%d pid=%d) (i=%d) value = %d\n", rank, getpid(), i, digit);
            write(fds[rank], buf, strlen(buf));
        }
    }

    for (int k = 0; k < NUM_WORKERS; k++)
        close(fds[k]);

    if (rank == 0) {
        wait(NULL);
        wait(NULL);
    }
    return 0;
}
