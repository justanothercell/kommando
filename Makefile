build: clean
	gcc -ggdb -rdynamic -o kommando $(shell find ./bootstrap -name "*.c") -Wall -Wextra -Werror -Wpointer-arith -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=unused-parameter

clean:
	rm -f kommando
	rm -f CACHELOG.txt
	rm -f MEMTRACE.txt

br: build run

run:
	name=$(basename $(file) .kdo); \
	./kommando $(shell ./kdolib/link) $$name.kdo $$name -cr

compile:
	name=$(basename $(file) .kdo); \
	./kommando $(shell ./kdolib/link) $$name.kdo $$name -c

help:
	./kommando --help

clean_examples:
	git clean -fX examples

test: build clean_examples
	@success=0; \
	fail=0; \
	all_files=$$(find ./examples -name "*.kdo"); \
	count=$$(echo $$all_files | wc -w); \
	index=0; \
	printf "Running tests...\n"; \
	for file in $$all_files; do \
		index=$$((index + 1)); \
		if make compile file=$$file > /dev/null 2>&1; then \
			printf "[\x1b[1;32mOK\x1b[0m] ($$index/$$count) $$file\n"; \
			success=$$((success + 1)); \
		else \
			printf "[\x1b[1;31mERROR\x1b[0m] ($$index/$$count) $$file\n"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	printf "Tests complete.\n"; \
	printf "\x1b[1;32msuccess: $$success\x1b[0m\t\x1b[1;31mfail: $$fail\x1b[0m\n"; \
	if (test $$fail -ne 0) \
	then \
		(exit 1); \
	fi \