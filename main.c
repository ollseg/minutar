
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "minutar.h"

/*
 * Simple test program to drive minutar
 *
 */
int main(int argc, const char** argv)
{
    FILE *input_file = NULL;

    if (argc != 2) {
        printf("usage\r\n");
        exit(1);
    }

    input_file = fopen(argv[1], "rb");
    if (NULL == input_file) {
        printf("open failed\r\n");
        exit(2);
    }

    if (!minutar_extract_all(input_file)) {
         printf("errors while processing the file\r\n");
         exit(3);
    }

    return 0;
}
