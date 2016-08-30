/*!
 *  \file util.h
 *  \brief Interface for utility functions used by the miutar module
 *
 */
#ifndef MINUTAR_UTIL_H_INCLUDED

#include <sys/stat.h>

#include "minutar.h"


/*!
 *  \fn bool canonicalize_paths(filedesc_t *file)
 *  \brief Canonicalizes all paths in a filedesc_t struct
 *
 *  Makes all paths absolute and strips leading slashes.
 *
 */
bool canonicalize_paths(filedesc_t *file);

/*!
 *  \fn bool path_mkdir(const char* path, mode_t mode)
 *  \brief makes sure a directory path exists, excluding the last path element
 *
 *  In the case of a trailing separator, the last element
 *  will be considered to be the empty string after it.
 *
 *  Will return true in the case of no path separator, 
 *  without attempting to create any directory.
 *  
 *  Returns true if the path exists, false on error,
 *  caller should check errno on failure for reason.
 *
 */
bool path_mkdir(const char* path, mode_t mode);

#endif /* MINUTAR_UTIL_H_INCLUDED */
