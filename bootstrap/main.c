#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "lib.h"
#include "parser.h"
LIB;
#include "compiler.h"

void cleanup_quit(str file, usize line, int code) {
    UNUSED(file); UNUSED(line); UNUSED(code);
}

int main(int argc, char* argv[]) {
    init_exit_handler(cleanup_quit);

    char* x = malloc(4);
    free(x);

    StrList args = list_new(StrList);
    args.length = argc;
    args.elements = argv;

    CompilerOptions options = build_args(&args);
    compile(options);

    if (options.run) {
        if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_CYAN_FG) "RUN" ANSI_RESET_SEQUENCE, "Running " ANSI(ANSI_WHITE_FG) "%s" ANSI_RESET_SEQUENCE " ...", options.source);
        fflush(stdout);
        int pid = fork();
        if (pid == 0) { // child
            char* argp_end = NULL;
            char* envp_end = NULL;
            execve(options.outname, &argp_end, &envp_end);
            unreachable("execve does not return");
        }
        pid_t status;
        waitpid(pid, &status, 0);

        if (status == -1) {
            if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_RED_FG) "RUN" ANSI_RESET_SEQUENCE, "Could not start execution");
        } else {
            if (options.verbosity >= 1) info(ANSI(ANSI_BOLD, ANSI_CYAN_FG) "RUN" ANSI_RESET_SEQUENCE, "Execution finished with code %u", status);
        }
    }

    quit(0);
}