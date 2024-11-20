#include <stdlib.h>
#include "defines.h"
#include "gc.h"
#include "str.h"
#include "terminal.h"
#include "exit.h"

#include <stdarg.h>
#include <stdio.h>
#include <execinfo.h>
#include <string.h>
#define __USE_GNU
#include <dlfcn.h>
#include "signal.h"

static void (*EXIT_FUNC)(str file, usize line, int code) = NULL;
#define INTERRUPT_MODULE ANSI(ANSI_BOLD, ANSI_RED_FG) "INTERRUPT" ANSI_RESET_SEQUENCE
void signal_handler(int signal) {
    printf("CAUGHT SIGNAL %u\n", signal);
    switch (signal) {
        case SIGINT:
            finfo(stderr, INTERRUPT_MODULE, "Caught signal SIGINT (%u)", signal);
            break;
        case SIGSEGV:
            finfo(stderr, INTERRUPT_MODULE, "Caught signal SIGSEGV (%u)", signal);
            break;
        case SIGFPE:
            finfo(stderr, INTERRUPT_MODULE, "Caught signal SIGFPE (%u)", signal);
            break;
        finfo(stderr, INTERRUPT_MODULE, "Caught signal (%u)", signal);
    }
    finfo(stderr, INTERRUPT_MODULE, "Stacktrace:");
    fprint_stacktrace(stderr);
    finfo(stderr, INTERRUPT_MODULE, "Exiting without cleanup");
    exit(signal);
}

void init_exit_handler(void (*exit_func)(str file, usize line, int code)) {
    if (EXIT_FUNC != NULL) panic("Exit handler may not be initialized twice");
    EXIT_FUNC = exit_func;
    signal(SIGINT, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE, signal_handler);
}

__attribute__((__noreturn__)) void __panic(str file, usize line, str fmt, ...) {
    fprint_prefix_raw(stderr, file, (int)line);
    fprintf(stderr, ANSI(ANSI_BOLD,ANSI_RED_FG)"Panic"ANSI_RESET_SEQUENCE": ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fprint_stacktrace(stderr);
    __quit(file, line, 1);
}

__attribute__((__noreturn__)) void __quit(str file, usize line, int code) {
    EXIT_FUNC(file, line, code);
    exit(code);
}

void fprint_stacktrace(FILE* file) {
    void* callstack[128];
    usize frames = backtrace(callstack, 128);
    str* trace = backtrace_symbols(callstack, frames);
    for (usize i = 1; i < frames; ++i) {
        Dl_info info;
        dladdr(callstack[i], &info);

        /* find first occurence of '(' or ' ' in message[i] and assume
        * everything before that is the file name. (Don't go beyond 0 though
        * (string terminator)*/
        usize p = 0;
        while(trace[i][p] != '(' && trace[i][p] != ' ' && trace[i][p] != 0) p += 1;

        char syscom[256];
        sprintf(syscom,"addr2line %p -f -e %.*s", (void*)((char*)callstack[i] - (char*)info.dli_fbase), (int)p, trace[i]);
        FILE* res = popen(syscom, "r");
        char result[256];
        usize read = fread(result, 1, 255, res);
        result[read-1] = '\0'; // removing \n
        pclose(res);
        
        StrList func_loc = split(result, '\n');
        str func = func_loc.elements[0];
        StrList loc = split(func_loc.elements[1], ':');
        if (strlen(func) > 0 && func[0] != '_') fprintf(file, "  %3lld: "ANSI(ANSI_BOLD)"%-32s"ANSI_RESET_SEQUENCE, i, func);
        else fprintf(file, "  %3lld: "ANSI(ANSI_FAINT)"%-32s"ANSI_RESET_SEQUENCE, i, func);
        if (!str_eq(loc.elements[0], "??")) {
            str path = loc.elements[0];
            str line = loc.elements[1];
            StrList dir_file = rsplitn(path, __PATH_SEP__, 1);
            str dir = dir_file.elements[0];
            str filename = dir_file.elements[1];
            fprintf(file, ANSI(ANSI_FAINT)" %s/"ANSI_RESET_SEQUENCE""ANSI(ANSI_BOLD)"%s:%s"ANSI_RESET_SEQUENCE"\n", dir, filename, line);
        }
        else fprintf(file, " "ANSI(ANSI_FAINT)"%s:%s"ANSI_RESET_SEQUENCE"\n", loc.elements[0], loc.elements[1]);
    }
    free(trace);
}