/*!
 *  \file util.c
 *  \brief Utility functions used by the minutar module
 *
 */
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "sassert.h"
#include "minutar.h"


/* TODO: Unicode support */
bool canonicalize_paths(filedesc_t *file)
{
    SASSERT(file != NULL);
    SASSERT(file->name != NULL);

    if (file->name[0] == '/') {
        memmove(file->name, file->name+1, strlen(file->name)+1);
    }

    /* TODO: canonicalize /../ elements */

    SASSERT(file->name[0] != '/');
    return true;
}

/* TODO: Unicode support */
bool path_mkdir(const char *path, mode_t mode)
{
    SASSERT(path != NULL);
    
    char *dir = strdup(path);
    if (NULL == dir)
        return false;

    char *end = strrchr(dir, '/');
    if (end != NULL) {
        *end = '\0';

        int err = mkdir(dir, mode);
        if (err != 0)
        {
            free(dir);
            return false;
        }
    }

    free(dir);
    return true;
}
