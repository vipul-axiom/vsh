/* vsh: a very small Unix shell.
 *
 * Reads a line, splits it on whitespace, and runs it. Two commands are
 * built in: cd and exit. Everything else is run with fork + execvp.
 * Input can come from a file with "cmd < file", output can go to a file
 * with "cmd > file" or be added to the end of one with "cmd >> file",
 * and commands can be joined with pipes: "cmd1 | cmd2 | cmd3".
 * There is no quoting and there are no variables.
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
#define MAX_STAGES 64  /* max commands in one pipeline (63 pipes) */

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

/* Handle redirection in the child process. Looks for these in args:
 *   "< file"  or "<file"   read stdin from file
 *   "> file"  or ">file"   write stdout to a file (created or truncated)
 *   ">> file" or ">>file"  append stdout to file (created if missing)
 * Each file is opened and put in place of stdin or stdout with dup2.
 * The operator and the file name are removed from args so the program
 * never sees them. If there is more than one of a kind, the last one
 * wins (like sh). Returns 0 on success, -1 on error (after a message). */
static int apply_redirect(char **args) {
    for (int i = 0; args[i]; i++) {
        const char *word = args[i];
        const char *op;  /* the operator: "<", ">" or ">>" */
        int flags;       /* how to open the file */
        int target;      /* STDIN_FILENO or STDOUT_FILENO */

        /* Check ">>" before ">", or ">>file" would be read as ">" with
         * the file name ">file". */
        if (word[0] == '>' && word[1] == '>') {
            op = ">>";
            flags = O_WRONLY | O_CREAT | O_APPEND;
            target = STDOUT_FILENO;
        } else if (word[0] == '>') {
            op = ">";
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            target = STDOUT_FILENO;
        } else if (word[0] == '<') {
            op = "<";
            flags = O_RDONLY;
            target = STDIN_FILENO;
        } else {
            continue;
        }

        const char *file;
        int skip;  /* how many words to remove from args */
        size_t len = strlen(op);
        if (word[len] != '\0') {
            file = word + len;  /* "<file": name follows the operator */
            skip = 1;
        } else {
            file = args[i + 1];  /* "< file": name is the next word */
            skip = 2;
            if (!file) {
                fprintf(stderr, "vsh: missing file name after '%s'\n", op);
                return -1;
            }
        }

        int fd = open(file, flags, 0644);  /* 0644 is only used if created */
        if (fd < 0) {
            fprintf(stderr, "vsh: %s: %s\n", file, strerror(errno));
            return -1;
        }
        if (dup2(fd, target) < 0) {
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

/* In a child process: set up any "<", ">" or ">>" redirection, then
 * replace this process with the program in args. Never returns. */
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

/* True if args has at least one "|" word. */
static int has_pipe(char **args) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "|") == 0) return 1;
    }
    return 0;
}

/* Close every end of every pipe in fds[0 .. npipes-1]. */
static void close_all_pipes(int fds[][2], int npipes) {
    for (int i = 0; i < npipes; i++) {
        close(fds[i][0]);
        close(fds[i][1]);
    }
}

/* Run "cmd1 | cmd2 | ... | cmdN". Splits args at each "|", forks one
 * child per command, and connects them with N-1 pipes. Each child still
 * applies its own "<", ">" or ">>" with apply_redirect. Built-ins are
 * not run here; the caller only reaches this for a line that has "|". */
static void run_pipeline(char **args) {
    char **stage[MAX_STAGES];
    int n = 0;  /* number of commands in the pipeline */

    stage[n++] = args;
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "|") == 0) {
            args[i] = NULL;  /* end this command where the "|" was */
            if (n >= MAX_STAGES) {
                fprintf(stderr, "vsh: too many pipes\n");
                return;
            }
            stage[n++] = &args[i + 1];
        }
    }

    /* Every stage must have a command. An empty one before a "|" or
     * after a "|" (including in the middle: "a | | b") is an error. */
    for (int i = 0; i < n; i++) {
        if (stage[i][0] != NULL) continue;
        if (i == 0)
            fprintf(stderr, "vsh: missing command before '|'\n");
        else
            fprintf(stderr, "vsh: missing command after '|'\n");
        return;
    }

    int npipes = n - 1;
    int fds[MAX_STAGES - 1][2];
    for (int i = 0; i < npipes; i++) {
        if (pipe(fds[i]) != 0) {
            perror("vsh: pipe");
            close_all_pipes(fds, i);  /* close any already created */
            return;
        }
    }

    pid_t pids[MAX_STAGES];
    int started = 0;
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("vsh: fork");
            close_all_pipes(fds, npipes);
            for (int j = 0; j < started; j++) waitpid(pids[j], NULL, 0);
            return;
        }
        if (pid == 0) {
            /* Child i: read from the previous pipe (if any), write to the
             * next pipe (if any). Then close every pipe end we still hold
             * so nothing is left open by accident. */
            if (i > 0) {
                if (dup2(fds[i - 1][0], STDIN_FILENO) < 0) {
                    perror("vsh: dup2");
                    _exit(1);
                }
            }
            if (i < n - 1) {
                if (dup2(fds[i][1], STDOUT_FILENO) < 0) {
                    perror("vsh: dup2");
                    _exit(1);
                }
            }
            close_all_pipes(fds, npipes);
            exec_child(stage[i]);
        }
        pids[started++] = pid;
    }

    /* Parent must close both ends of every pipe. If a write end stayed
     * open here, the next command would never see end of input and hang. */
    close_all_pipes(fds, npipes);

    int status;
    for (int i = 0; i < started; i++) {
        if (waitpid(pids[i], &status, 0) < 0) perror("vsh: waitpid");
    }
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
        } else if (has_pipe(args)) {
            run_pipeline(args);  /* built-ins are not run in a pipeline */
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
