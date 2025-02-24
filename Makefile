GCC_FLAGS = -fdebug-prefix-map=$(shell pwd)=.
GCC_RELEASE_FLAGS = -O3 -Wno-array-bounds
GCC_WARNINGS = -Wall -Wextra -Werror -Wpointer-arith -Wno-unused-parameter -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=unused-function -Wno-error=unused-parameter -Wno-error=unused-result

build: clean
	@echo Building compiler...
	@gcc -ggdb -rdynamic -o kommando $(shell find ./bootstrap -name "*.c") $(GCC_FLAGS) $(GCC_WARNINGS)
	@echo Done!

build_release: clean
	@echo Building compiler \(release\) ...
	@gcc -ggdb -rdynamic -o kommando $(shell find ./bootstrap -name "*.c") $(GCC_FLAGS) $(GCC_WARNINGS) $(GCC_RELEASE_FLAGS)
	@echo Done!

clean:
	@rm -f kommando
	@rm -f CACHELOG.txt
	@rm -f MEMTRACE.txt
	@rm -f out.txt

br: build run

run:
	@name=$(basename $(file) .kdo); \
	./kommando $(shell ./kdolib/link) $$name.kdo $$name -cr $(flags)

compile:
	@name=$(basename $(file) .kdo); \
	./kommando $(shell ./kdolib/link) $$name.kdo $$name -c $(flags)

help:
	@if [ ! -f ./kommando ]; then \
		make --no-print-directory build; \
	fi
	@./kommando --help
	@echo Additional make commands:
	@echo '    make test  <args?>   - test compile examples'
	@echo '               verbose=1 - show compile errors in test'
	@echo '               release=1 - run compiler in release mode'

clean_examples:
	@git clean -fX examples >/dev/null

test: clean_examples
	@if [ -v release ]; then \
		make --no-print-directory build_release; \
	else \
		make --no-print-directory build; \
	fi; \
	@success=0; \
	fail=0; \
	all_files=$$(find ./examples -name "*.kdo"); \
	count=$$(echo $$all_files | wc -w); \
	index=0; \
	printf "\x1b[1mRunning tests...\x1b[0m\n"; \
	for file in $$all_files; do \
		index=$$((index + 1)); \
		if [ -v verbose ]; then \
			if make --no-print-directory compile file=$$file flags="--silent"; then \
				printf "[\x1b[1;32mOK\x1b[0m] ($$index/$$count) $$file\n"; \
				success=$$((success + 1)); \
			else \
				printf "[\x1b[1;31mERROR\x1b[0m] ($$index/$$count) $$file\n"; \
				fail=$$((fail + 1)); \
			fi; \
		else \
			if make --no-print-directory compile file=$$file flags="--silent" > /dev/null 2>&1; then \
				printf "[\x1b[1;32mOK\x1b[0m] ($$index/$$count) $$file\n"; \
				success=$$((success + 1)); \
			else \
				printf "[\x1b[1;31mERROR\x1b[0m] ($$index/$$count) $$file\n"; \
				fail=$$((fail + 1)); \
			fi; \
		fi \
	done; \
	printf "\x1b[1mTests complete.\x1b[0m\n"; \
	printf "\x1b[1;32msuccess: $$success\x1b[0m\t\x1b[1;31mfail: $$fail\x1b[0m\n"; \
	if (test $$fail -ne 0); then \
		(exit 1); \
	fi \

stats:
	@echo "bootstrap: $(shell cat $(shell find ./bootstrap -type f \( -name '*.c' -o -name '*.h' \)) | wc -l) lines (*.c, *.h)"
	@echo "examples:  $(shell cat $(shell find ./examples -type f \( -name '*.kdo' \)) | wc -l) lines (*.kdo)"
	@echo "kdolib:    $(shell cat $(shell find ./kdolib -type f \( -name '*.kdo' \)) | wc -l) lines (*.kdo)"
	@echo "docs:      $(shell cat $(shell find ./docs/src -type f \( -name '*.md' \)) | wc -l) lines (*.md)"