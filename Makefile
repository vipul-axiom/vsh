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
	@rm -f test.out

clean:
	rm -f vsh test.out

.PHONY: all test clean
