build: clean
	gcc -ggdb -rdynamic -o kommando $(shell find ./bootstrap -name "*.c") -Wall -Wextra -Werror -Wpointer-arith -Wno-error=unused-variable -Wno-error=unused-parameter

clean:
	rm -f kommando

cr: build run

run:
	name=$(basename $(file) .kdo); \
	./kommando $(shell ./kdolib/link) $$name.kdo $$name -cr

compile:
	name=$(basename $(file) .kdo); \
	./kommando $(shell ./kdolib/link) $$name.kdo $$name -c

help:
	./kommando --help

test: build
	@success=0; \
	fail=0; \
	for file in $(shell find ./examples -name "*.kdo"); do \
		echo "Running test: $$file"; \
		if make compile file=$$file > /dev/null; then \
			echo "Successfully compiled $$file"; \
			success=$$((success + 1)); \
		else \
			echo "Error compiling $$file"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo "$$success examples compiled successfully, $$fail failed";