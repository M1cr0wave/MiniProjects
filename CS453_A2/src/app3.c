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
    /* Output files are opened by comp3 and inherited on fixed descriptors.
       app3 never opens an output file and never receives fds via argv. */
    int fds[NUM_WORKERS] = {3, 4, 5};

#ifdef TRY_WRITE_OPEN
    /* Demo only: attempt to open an output file for writing -> SIGSYS */
    int bad = open("output0.txt", O_WRONLY);
    printf("open for writing returned %d (should never be reached)\n", bad);
#endif

    /// read the ID from id.txt, opened read-only
    char id[ID_SIZE];
    int idfd = open("id.txt", O_RDONLY);
    if (idfd < 0) { perror("open id.txt"); return 1; }
    ssize_t n = read(idfd, id, ID_SIZE);
    close(idfd);
    if (n <= 0) { fprintf(stderr, "id.txt is empty\n"); return 1; }

    /// keep only the leading digits (drops trailing newline); at most 8
    int upper_limit = 0;
    while (upper_limit < n && id[upper_limit] >= '0' && id[upper_limit] <= '9')
        upper_limit++;

    /// the parent creates two workers; every process gets a rank of 0, 1 or 2
    int rank = 0;
    if (fork() == 0)
        rank = 1;
    else if (fork() == 0)
        rank = 2;

    /// each process writes the digits whose value mod 3 equals its rank
    for (int i = 0; i < upper_limit; i++) {
        int digit = id[i] - 0x30;
        if (digit % 3 == rank) {
            sprintf(buf, "(rank=%d pid=%d) (i=%d) value = %d\n", rank, getpid(), i, digit);
            write(fds[rank], buf, strlen(buf));
        }
    }

    for (int k = 0; k < NUM_WORKERS; k++)
        close(fds[k]);

    /// the parent waits for both workers to finish
    if (rank == 0) {
        wait(NULL);
        wait(NULL);
    }
    return 0;
}
