build: clean
	gcc -ggdb -rdynamic -o kommando $(shell find ./bootstrap -name "*.c") -Wall -Wextra -Werror -Wpointer-arith -Wno-error=unused-variable -Wno-error=unused-parameter

clean:
	rm -f kommando

cr: build
	./kommando $(shell ./kdolib/link) $(file).kdo $(file) -cr

run:
	./kommando $(shell ./kdolib/link) $(file).kdo $(file) -cr