/* vsh: a very small Unix shell.
 *
 * Reads a line, splits it on whitespace, and runs it. Two commands are
 * built in: cd and exit. Everything else is run with fork + execvp.
 * Output can be sent to a file with "cmd > file" (or "cmd >file").
 * There are no pipes, input redirection, quoting, or variables.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define DELIMS " \t\r\n\a"

/* Split line into a NULL-terminated array of words. The words point
 * into line, which strtok modifies. Returns NULL if out of memory. */
static char **split_line(char *line) {
    size_t cap = 8, n = 0;
    char **args = malloc(cap * sizeof *args);
    if (!args) return NULL;

    for (char *tok = strtok(line, DELIMS); tok; tok = strtok(NULL, DELIMS)) {
        if (n + 1 >= cap) {  /* leave room for the final NULL */
            cap *= 2;
            char **next = realloc(args, cap * sizeof *args);
            if (!next) {
                free(args);
                return NULL;
            }
            args = next;
        }
        args[n++] = tok;
    }
    args[n] = NULL;
    return args;
}

/* Built-in cd. With no argument, go to $HOME. */
static void builtin_cd(char **args) {
    const char *dir = args[1];
    if (!dir) {
        dir = getenv("HOME");
        if (!dir) {
            fprintf(stderr, "vsh: cd: HOME not set\n");
            return;
        }
    }
    if (chdir(dir) != 0) perror("vsh: cd");
}

/* Handle output redirection in the child process. Looks for "> file" or
 * ">file" in args, opens the file, and makes it the child's stdout. The
 * ">" and the file name are removed from args so the program never sees
 * them. If there is more than one, the last one wins (like sh).
 * Returns 0 on success, -1 on error (after printing a message). */
static int apply_redirect(char **args) {
    for (int i = 0; args[i]; i++) {
        const char *file;
        int skip;  /* how many words to remove from args */

        if (strcmp(args[i], ">") == 0) {
            file = args[i + 1];
            skip = 2;
            if (!file) {
                fprintf(stderr, "vsh: missing file name after '>'\n");
                return -1;
            }
        } else if (args[i][0] == '>' && args[i][1] == '>') {
            fprintf(stderr, "vsh: '>>' (append) is not supported\n");
            return -1;
        } else if (args[i][0] == '>') {
            file = args[i] + 1;  /* ">file": name follows the '>' */
            skip = 1;
        } else {
            continue;
        }

        int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "vsh: %s: %s\n", file, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("vsh: dup2");
            close(fd);
            return -1;
        }
        close(fd);

        /* Shift the rest of args left, over the removed words. */
        int j = i;
        do {
            args[j] = args[j + skip];
        } while (args[j++] != NULL);
        i--;  /* look at position i again: it now holds a new word */
    }
    return 0;
}

/* Run an external program and wait for it to finish. */
static void run_program(char **args) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("vsh: fork");
        return;
    }
    if (pid == 0) {
        if (apply_redirect(args) != 0) _exit(1);
        if (args[0] == NULL) _exit(0);  /* just "> file": file is created */
        execvp(args[0], args);
        perror(args[0]);  /* only reached if execvp failed */
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) perror("vsh: waitpid");
}

int main(void) {
    char *line = NULL;
    size_t cap = 0;

    for (;;) {
        printf("vsh> ");
        fflush(stdout);

        if (getline(&line, &cap, stdin) == -1) {
            printf("\n");  /* EOF (Ctrl-D): end the prompt line and quit */
            break;
        }

        char **args = split_line(line);
        if (!args) {
            fprintf(stderr, "vsh: out of memory\n");
            break;
        }

        if (args[0] == NULL) {
            /* empty line: nothing to do */
        } else if (strcmp(args[0], "exit") == 0) {
            free(args);
            break;
        } else if (strcmp(args[0], "cd") == 0) {
            builtin_cd(args);
        } else {
            run_program(args);
        }
        free(args);
    }

    free(line);
    return 0;
}
