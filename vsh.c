/* vsh: a very small Unix shell.
 *
 * Reads a line, splits it on whitespace, and runs it. Two commands are
 * built in: cd and exit. Everything else is run with fork + execvp.
 * Output can be sent to a file with "cmd > file" (or "cmd >file"), and
 * two commands can be joined with one pipe: "cmd1 | cmd2".
 * There is no input redirection, quoting, or variables.
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

/* In a child process: set up any "> file", then replace this process
 * with the program in args. Never returns. */
static void exec_child(char **args) {
    if (apply_redirect(args) != 0) _exit(1);
    if (args[0] == NULL) _exit(0);  /* just "> file": file is created */
    execvp(args[0], args);
    perror(args[0]);  /* only reached if execvp failed */
    _exit(127);
}

/* Run an external program and wait for it to finish. */
static void run_program(char **args) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("vsh: fork");
        return;
    }
    if (pid == 0) exec_child(args);
    int status;
    if (waitpid(pid, &status, 0) < 0) perror("vsh: waitpid");
}

/* Look for a "|" word in args. Returns its index, -1 if there is none,
 * or -2 if there is more than one (after printing an error). */
static int find_pipe(char **args) {
    int pos = -1;
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "|") == 0) {
            if (pos != -1) {
                fprintf(stderr, "vsh: only one '|' is supported\n");
                return -2;
            }
            pos = i;
        }
    }
    return pos;
}

/* Run "cmd1 | cmd2". pos is the index of the "|" in args. The output of
 * the first command goes into a pipe, and the second reads from it. */
static void run_pipeline(char **args, int pos) {
    char **left = args;
    char **right = args + pos + 1;
    args[pos] = NULL;  /* end the first command where the "|" was */

    if (left[0] == NULL) {
        fprintf(stderr, "vsh: missing command before '|'\n");
        return;
    }
    if (right[0] == NULL) {
        fprintf(stderr, "vsh: missing command after '|'\n");
        return;
    }

    int fds[2];  /* fds[0] is the read end, fds[1] the write end */
    if (pipe(fds) != 0) {
        perror("vsh: pipe");
        return;
    }

    pid_t first = fork();
    if (first < 0) {
        perror("vsh: fork");
        close(fds[0]);
        close(fds[1]);
        return;
    }
    if (first == 0) {
        /* First child: stdout goes into the pipe. */
        if (dup2(fds[1], STDOUT_FILENO) < 0) {
            perror("vsh: dup2");
            _exit(1);
        }
        close(fds[0]);
        close(fds[1]);
        exec_child(left);
    }

    pid_t second = fork();
    if (second < 0) {
        perror("vsh: fork");
        close(fds[0]);
        close(fds[1]);
        waitpid(first, NULL, 0);
        return;
    }
    if (second == 0) {
        /* Second child: stdin comes from the pipe. */
        if (dup2(fds[0], STDIN_FILENO) < 0) {
            perror("vsh: dup2");
            _exit(1);
        }
        close(fds[0]);
        close(fds[1]);
        exec_child(right);
    }

    /* The parent must close both ends too. If the write end stayed open
     * here, the second command would never see end of input and hang. */
    close(fds[0]);
    close(fds[1]);

    int status;
    if (waitpid(first, &status, 0) < 0) perror("vsh: waitpid");
    if (waitpid(second, &status, 0) < 0) perror("vsh: waitpid");
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

        int pos = args[0] ? find_pipe(args) : -1;
        if (args[0] == NULL) {
            /* empty line: nothing to do */
        } else if (pos == -2) {
            /* more than one '|': error already printed */
        } else if (pos >= 0) {
            run_pipeline(args, pos);  /* built-ins are not run in a pipeline */
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
