[![CI](https://github.com/vipul-axiom/vsh/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/vipul-axiom/vsh/actions/workflows/ci.yml)

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
- Output redirection: `cmd > file` (or `cmd >file`) sends stdout to a file,
  creating or truncating it
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
$ printf 'echo hello from vsh\ncd /\npwd\nls /nonexistent\nnotacommand\ncd\npwd\necho saved to a file > out.txt\ncat out.txt\nexit\n' | HOME=/tmp ./vsh
vsh> hello from vsh
vsh> vsh> /
vsh> ls: cannot access '/nonexistent': No such file or directory
vsh> notacommand: No such file or directory
vsh> vsh> /tmp
vsh> vsh> saved to a file
vsh>
```

(The prompt is printed before each command is read, so when input is piped
the output appears right after `vsh> `. `cd` prints nothing, so its prompt
is followed directly by the next one, and so is `echo ... > out.txt`,
because its output went into `out.txt` instead of the terminal; `cat`
then shows it. `HOME=/tmp` is set here only so the example shows `cd`
with no argument going to `$HOME`.)

## Limitations

This is a learning project, so it is deliberately small:

- No pipes (`|`), input redirection (`<`), or appending (`>>`); only `>` is supported
- No quoting or escaping: `echo "a b"` passes `"a` and `b"` as two words
- No environment variables (`$HOME`), globbing (`*.c`), or `~` expansion
- No background jobs (`&`), job control, or signal handling
- No history or line editing

## License

MIT. See [LICENSE](LICENSE).
