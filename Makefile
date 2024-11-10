build: clean
	gcc -ggdb -rdynamic -o kommando $(shell find ./bootstrap -name "*.c") -Wall -Wextra -Werror -Wno-unused -Wpointer-arith

clean:
	rm -f kommando

cr: build
	./kommando $(shell ./kdolib/link) $(file).kdo $(file) -cr

run:
	./kommando $(shell ./kdolib/link) $(file).kdo $(file) -cr