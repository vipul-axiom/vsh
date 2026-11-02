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
- Appending: `cmd >> file` (or `cmd >>file`) adds stdout to the end of a
  file, creating it if needed
- Input redirection: `cmd < file` (or `cmd <file`) makes the file the
  command's stdin; they can be combined, e.g. `sort < in.txt > out.txt`,
  and if the same kind is given twice the last one wins
- A single pipe: `cmd1 | cmd2` sends the output of `cmd1` into `cmd2`;
  redirection works with it too, e.g. `cat < in.txt | sort >> log.txt`
  (`<` on the first command, `>` or `>>` on the second); the `|` must be
  a separate word, with spaces around it
- Errors reported with `perror`
- Ctrl-D (end of input) exits cleanly

## Build

You need a C compiler (`cc`/`gcc`/`clang`) and `make`.

```
make          # builds ./vsh
make test     # builds, then runs every test in tests/ (see Testing below)
make clean    # removes the binary
```

## Usage

Run `./vsh` and type commands. Here is a real session, with the commands
piped in so the output can be shown in one go:

```
$ printf 'echo hello from vsh\ncd /\npwd\nls /nonexistent\nnotacommand\ncd\npwd\necho saved to a file > out.txt\ncat out.txt\necho pear > fruit.txt\necho apple >> fruit.txt\necho fig >>fruit.txt\ncat fruit.txt\nsort < fruit.txt\nsort < fruit.txt > sorted.txt\ncat sorted.txt\nwc -l < nofile.txt\necho one two three | wc -w\necho one two three | wc -w > count.txt\ncat < fruit.txt | wc -l >> count.txt\ncat count.txt\n| wc -l\nexit\n' | HOME=/tmp ./vsh
vsh> hello from vsh
vsh> vsh> /
vsh> ls: cannot access '/nonexistent': No such file or directory
vsh> notacommand: No such file or directory
vsh> vsh> /tmp
vsh> vsh> saved to a file
vsh> vsh> vsh> vsh> pear
apple
fig
vsh> apple
fig
pear
vsh> vsh> apple
fig
pear
vsh> vsh: nofile.txt: No such file or directory
vsh> 3
vsh> vsh> vsh> 3
3
vsh> vsh: missing command before '|'
vsh> 
```

(The prompt is printed before each command is read, so when input is piped
the output appears right after `vsh> `. `cd` prints nothing, so its prompt
is followed directly by the next one, and so is any command whose output
went into a file instead of the terminal, like `echo ... > out.txt`;
`cat` then shows what the file holds. `>>` adds `apple` and `fig` after
`pear` instead of replacing it, `sort < fruit.txt` reads the file as its
input, and `sort < fruit.txt > sorted.txt` does both at once. A missing
input file is reported by vsh itself, and the command is not run. The
pipe `echo one two three | wc -w` counts the words coming out of `echo`;
with `> count.txt` the count goes into the file instead, and the last
pipe reads `fruit.txt` with `<` and appends its line count to
`count.txt` with `>>`. `HOME=/tmp` is set here only so the example
shows `cd` with no argument going to `$HOME`.)

## Testing

`make test` runs the tests in the `tests/` folder. Each test is a pair
of files:

- `tests/NAME.in`: the lines fed to `./vsh` on stdin, as if typed
- `tests/NAME.expected`: everything vsh should print for them, stdout
  and stderr together (prompts included)

Each test runs in its own new temporary directory (made with
`mktemp -d`), with `HOME` set to that directory, so any files it creates
are thrown away afterwards and `cd` with no argument goes somewhere
known. Each run is stopped after 5 seconds with `timeout`, in case a pipe
is left open and vsh hangs. The output is compared with `diff`, and a
`PASS NAME` or `FAIL NAME` line is printed for each test. To add a test,
add a new `.in` / `.expected` pair; the Makefile picks it up on its own.

## Limitations

This is a learning project, so it is deliberately small:

- Only one pipe per line: `a | b | c` is an error
- The `|` needs spaces around it: `ls|wc` is read as one word
- No built-ins in pipelines: `cd` and `exit` are not run inside one
- No stderr redirection (`2>`) or here-documents (`<<`)
- No quoting or escaping: `echo "a b"` passes `"a` and `b"` as two words
- No environment variables (`$HOME`), globbing (`*.c`), or `~` expansion
- No background jobs (`&`), job control, or signal handling
- No history or line editing

## License

MIT. See [LICENSE](LICENSE).

## What I learned

- Starting programs with fork and execvp, and waiting for them with waitpid.
- Redirecting input and output with open and dup2.
- Connecting two commands with pipe, and closing unused pipe ends so the reader doesn't hang.

## Next up

- Support more than one pipe in a line (cmd1 | cmd2 | cmd3).
