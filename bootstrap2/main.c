//#include "lib.h"
#include "lib.h"
#include <stdlib.h>
#include <stdlib.h>

void cleanup_quit(str file, usize line, int code) {
    gc_end();
}

typedef struct Foo {
    struct Foo* child;
    struct Foo* parent;
} Foo;

void garbage() {
    Foo* x = gc_malloc(sizeof(Foo));
    x->child = NULL;
    x->parent = NULL;

    for(usize i = 0;i < 2000;i++) {

        Foo* o = gc_malloc(sizeof(Foo));
        o->child = NULL;
        o->parent = NULL;

        if (i % 2 == 0) {
            o->child = x;
            x->parent = o;
        } else {
            x->child = o;
            o->parent = x;
            x = o;
        }
    }

    gc_collect();
    
    gc_collect();
}

int main() {
    init_exit_handler(cleanup_quit);
    gc_begin();

    garbage();

    quit(0);
}