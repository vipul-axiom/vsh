# vsh

A tiny Unix shell written in C99 with POSIX calls. I wrote it to learn how
a shell actually starts programs: it reads a line, splits it into words,
then uses `fork`, `execvp`, and `waitpid` to run the command.

## Features

- A `vsh> ` prompt loop
- Line reading with `getline`
- Words split on spaces and tabs
- External commands run with `fork` / `execvp` / `waitpid`
- Built-ins: `cd [dir]` (no argument goes to `$HOME`) and `exit`
- Errors reported with `perror`
- Ctrl-D (end of input) exits cleanly

## Build

You need a C compiler (`cc`/`gcc`/`clang`) and `make`.

```
make          # builds ./vsh
make test     # builds, then pipes a few commands into ./vsh and checks the output
make clean    # removes the binary
```

## Usage

Run `./vsh` and type commands. Here is a real session, with the commands
piped in so the output can be shown in one go:

```
$ printf 'echo hello from vsh\ncd /tmp\npwd\nls /nonexistent\nnotacommand\ncd\npwd\nexit\n' | ./vsh
vsh> hello from vsh
vsh> vsh> /tmp
vsh> ls: cannot access '/nonexistent': No such file or directory
vsh> notacommand: No such file or directory
vsh> vsh> /home/box
vsh>
```

(The prompt is printed before each command is read, so when input is piped
the output appears right after `vsh> `. `cd` prints nothing, so its prompt
is followed directly by the next one.)

## Limitations

This is a learning project, so it is deliberately small:

- No pipes (`|`) or redirection (`<`, `>`, `>>`)
- No quoting or escaping: `echo "a b"` passes `"a` and `b"` as two words
- No environment variables (`$HOME`), globbing (`*.c`), or `~` expansion
- No background jobs (`&`), job control, or signal handling
- No history or line editing

## License

MIT. See [LICENSE](LICENSE).
