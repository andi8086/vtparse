#include <sys/types.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    int master;
    pid_t pid = forkpty(&master, NULL, NULL, NULL);

    if (pid == -1) {
        perror("forkpty");
        return EXIT_FAILURE;
    }

    if (!pid) {
        char* exec_argv[] = {"mc", NULL};
        execvp(exec_argv[0], exec_argv);
        perror("CHILD: execvp");
        return EXIT_FAILURE;
    }

    char text[] = "Hello, world!\n";
    char* text_ptr = text;
    size_t bytes_left = sizeof(text) - 1;

/*    while (bytes_left > 0) {
        ssize_t bytes_written = write(master, text_ptr, bytes_left);

        if (bytes_written == -1) {
            perror("PARENT: write");
            return EXIT_FAILURE;
        }

        bytes_left -= bytes_written;
        text_ptr += bytes_written;
    }
*/
    while (1) {
        int rcv;

        char buffer[128];
        while ((rcv = read(master, buffer, sizeof(buffer))) > 0) {
                printf("%s", buffer);
        }


        /*if (close(master) == -1) {
                perror("PARENT: close");
                return EXIT_FAILURE;
        } */
        while ((rcv = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
                write(master, buffer, rcv);
        }

        if (waitpid(pid, NULL, 0) == -1) {
                perror("PARENT: waitpid");
                return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
