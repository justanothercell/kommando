#pragma once

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    for(int i = 0;i < argc;i++) {
        printf("%s\n", argv[i]);
    }
    printf("Hello, World!\n");
    system("echo hi");

    FILE *fptr;
    fptr = fopen("main.c", "r");

    char buffer[2];
    
    int i = 0;

    while(fgets(buffer, 2, fptr)) {
        //printf("%d: %s", i, buffer);
        i += 1;
    }

    fclose(fptr);

    return 0;
}