CC = cc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -O2

all: vsh

vsh: vsh.c
	$(CC) $(CFLAGS) -o vsh vsh.c

test: vsh
	@printf 'echo hi\ncd /tmp\npwd\nexit\n' | ./vsh > test.out 2>&1; \
	printf 'vsh> hi\nvsh> vsh> /tmp\nvsh> ' | diff -u - test.out \
		&& echo "PASS basic commands" || { rm -f test.out; exit 1; }
	@printf 'nosuchcommand_vsh\n' | ./vsh > test.out 2>&1; \
	grep -q 'nosuchcommand_vsh: No such file or directory' test.out \
		&& echo "PASS missing command error" || { cat test.out; rm -f test.out; exit 1; }
	@printf 'cd\npwd\n' | HOME=/tmp ./vsh > test.out 2>&1; \
	printf 'vsh> vsh> /tmp\nvsh> \n' | diff -u - test.out \
		&& echo "PASS cd with no argument and EOF" || { rm -f test.out; exit 1; }
	@dir=$$(mktemp -d) && vsh="$$(pwd)/vsh" && cd "$$dir" \
	&& printf 'echo hello > out.txt\ncat out.txt\necho world >out2.txt\ncat out2.txt\n' \
		| "$$vsh" > test.out 2>&1 \
	&& printf 'vsh> vsh> hello\nvsh> vsh> world\nvsh> \n' | diff -u - test.out \
	&& printf 'hello\n' | cmp -s - out.txt \
	&& echo "PASS output redirection" \
	|| { cd /; rm -rf "$$dir"; exit 1; }; \
	cd /; rm -rf "$$dir"
	@printf 'echo oops >\n' | ./vsh > test.out 2>&1; \
	grep -q "vsh: missing file name after '>'" test.out \
		&& echo "PASS redirection missing file name" || { cat test.out; rm -f test.out; exit 1; }
	@rm -f test.out

clean:
	rm -f vsh test.out

.PHONY: all test clean
