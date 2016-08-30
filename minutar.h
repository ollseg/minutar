#ifndef MINUTAR_H_INCLUDED
#define MINUTAR_H_INCLUDED

/*!
 *  \file minutar.h
 *  \brief Interface of the minutar module
 *
 */

#include <stdio.h>
#include <stdbool.h>

/*!
 * \enum typeflag_t
 * \brief Datastructure that indicates the type of a file node in a tape archive
 * 
 * Only values TYPEFLAG_REG to TYPEFLAG_CONT inclusive and TYPEFLAG_EOA
 *  will be returned from the public interface to minutar.
 *
 */
typedef enum {
    TYPEFLAG_AREG =  '\0',  /*! legacy value of REGular file, should not be used */
    TYPEFLAG_REG =   '0',   /*! REGular file, normal file node with contents */
    TYPEFLAG_LNK =   '1',   /*! LiNK node, represents a hardlink to a file */
    TYPEFLAG_SYM =   '2',   /*! SYMbolic link, represents a symlink to a path */
    TYPEFLAG_CHR =   '3',   /*! CHaRacter device special file */
    TYPEFLAG_BLK =   '4',   /*! BLocK device special file */
    TYPEFLAG_DIR =   '5',   /*! DIRectory, represents an empty directory */
    TYPEFLAG_FIFO =  '6',   /*! FIFO special file, represents a UNIX socket file */
    TYPEFLAG_CONT =  '7',   /*! CONTinuous file, a special form of REG */
    TYPEFLAG_GNUK =  'K',   /*! GNU LongLink target, internal value that shouldn't be used */
    TYPEFLAG_GNUL =  'L',   /*! GNU LongLink name, internal value that shouldn't be used */
    TYPEFLAG_XHD =   'x',   /*! POSIX eXtended HeaDer, internal value that shouldn't be used */
    TYPEFLAG_XGL =   'g',   /*! POSIX eXtended GLobal header, internal value that shouldn't be used */
    TYPEFLAG_EOA =  0xfe,   /*! End Of Archive, returned when a tar file end-of-archive indicator is reached */

    TYPEFLAG_UNKNOWN = 0xff /*! UNKNOWN type, internal value that shouldn't be used */
} typeflag_t;

/*!
 * \struct filedesc_t
 * \brief Datastructure that describes a file node in a tape archive
 * 
 */
typedef struct filedesc_s {
    char *name;             /*! the name of the file node as a malloc()ed utf-8 string */
    char *linktarget;       /*! the target of a link type node as a malloc()ed utf-8 string */
    char *prefix;           /*! a prefix to be prepended to the file name as a malloc()ed utf-8 string */
    typeflag_t type;        /*! the type of the file node */
    size_t size;            /*! the size of the contents of the file node */
    size_t mode;            /*! bitfield of the file node access mode */
    time_t mtime;           /*! the unix epoch-time representation of the file node modification time */
    time_t atime;           /*! the unix epoch-time representation of the file node last access time */
    time_t ctime;           /*! the unix epoch-time representation of the file node metadata change time */
    size_t devmajor;        /*! the major type of a block/charachet device node */
    size_t devminor;        /*! the minor type of a block/charachet device node */
} filedesc_t;


/*!
 *  \fn bool minutar_get_next_file(FILE *tarfile, filedesc_t *output_nextfile)
 *  \brief Gets the next file in the tape archive file
 *
 *  Returns true on success and outputs a datastructure
 *  that describes the next file in the tape archive.
 *  When the end of the archive is reached, the "type" field
 *  of the output datastructure will have value TYPEFLAG_EOA.
 *  Returns false if no valid next file could be read.
 *
 *  The function advances the "tarfile" read pointer so
 *  that it is ready to read the file contents, if any.
 *
 *  If the function returns true, the caller must call
 *  minutar_free_filedesc() too free the output datastructure.
 *
 */
bool minutar_get_next_file(FILE *tarfile, filedesc_t *output_nextfile);

/*!
 *  \fn bool minutar_skip_file(FILE *tarfile, const filedesc_t skip_file)
 *  \brief Skips a file in the tape archive
 *
 *  Returns true on successful skip, if function returns
 *  false, caller should check feof(), ferror() and errno.
 *
 */
bool minutar_skip_file(FILE *tarfile, const filedesc_t skip_file);

/*!
 *  \fn void minutar_free_filedesc(filedesc_t *file)
 *  \brief Frees the datastructure returned by minutar_get_next_file
 *
 *  This function must be called on each datastructure that
 *  is output by minutar_get_next_file when it returns true.
 *
 */
void minutar_free_filedesc(filedesc_t *file);

/*!
 * \fn bool minutar_extract_all(FILE *tarfile)
 * \brief Extract all files in a tape archive
 *
 * Returns true if all files are successullfy extracted.
 *
 */
bool minutar_extract_all(FILE *tarfile);

#endif /* MINUTAR_H_INCLUDED */
