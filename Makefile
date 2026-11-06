CC = cc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -O2
# Stop a test after 5 seconds if it hangs (e.g. a pipe left open).
TIMEOUT = $(shell command -v timeout >/dev/null 2>&1 && echo timeout 5)

all: vsh

vsh: vsh.c
	$(CC) $(CFLAGS) -o vsh vsh.c

# Each test is a pair of files in tests/:
#   tests/NAME.in        the lines typed into vsh (fed to it on stdin)
#   tests/NAME.expected  everything vsh should print (stdout and stderr)
# Each test runs in its own new temporary directory, with HOME set to
# that directory, so files a test creates can't clash with another test
# and "cd" with no argument goes somewhere known. The directory is
# removed afterwards.
test: vsh
	@status=0; vsh="$$(pwd)/vsh"; \
	for t in tests/*.in; do \
		name=$$(basename "$$t" .in); \
		dir=$$(mktemp -d); \
		if (cd "$$dir" && HOME="$$dir" $(TIMEOUT) "$$vsh" 2>&1) < "$$t" \
			| diff -u "tests/$$name.expected" -; then \
			echo "PASS $$name"; \
		else \
			echo "FAIL $$name"; status=1; \
		fi; \
		rm -rf "$$dir"; \
	done; \
	exit $$status

clean:
	rm -f vsh

.PHONY: all test clean
