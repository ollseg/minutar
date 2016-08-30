/*!
 *  \file minutar.c
 *  \brief Implementation of the minutar module
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include <unistd.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "sassert.h"
#include "minutar.h"
#include "util.h"

#ifdef DEBUG
#define RETURN_FALSE_IF(x) do{ if((x)){ fprintf(stderr, "%s returned false on line %u: (%s)\r\n", __FUNCTION__, __LINE__, #x); return false; } }while(0)
#define GOTO_CLEANUP_IF(x) do{ if((x)){ fprintf(stderr, "%s returned false on line %u: (%s)\r\n", __FUNCTION__, __LINE__, #x); goto cleanup; } }while(0)
#else /* DEBUG */
#define RETURN_FALSE_IF(x) do{ if((x)){ return false; } }while(0)
#define GOTO_CLEANUP_IF(x) do{ if((x)){ goto cleanup; } }while(0)
#endif /* DEBUG */

static const size_t TAR_BLOCKSIZE = 512;
static const size_t TAR_HEADER_NAME_OFFSET = 0;
static const size_t TAR_HEADER_NAME_WIDTH = 100;
static const size_t TAR_HEADER_MODE_OFFSET = 100;
static const size_t TAR_HEADER_MODE_WIDTH = 8;
static const size_t TAR_HEADER_SIZE_OFFSET = 124;
static const size_t TAR_HEADER_SIZE_WIDTH = 12;
static const size_t TAR_HEADER_MTIME_OFFSET = 136;
static const size_t TAR_HEADER_MTIME_WIDTH = 12;
static const size_t TAR_HEADER_CHKSUM_OFFSET = 148;
static const size_t TAR_HEADER_CHKSUM_WIDTH = 8;
static const size_t TAR_HEADER_TYPE_OFFSET = 156;
static const size_t TAR_HEADER_LINK_OFFSET = 157;
static const size_t TAR_HEADER_LINK_WIDTH = 100;
static const size_t TAR_HEADER_VERSION_OFFSET = 263;
static const size_t TAR_HEADER_MAGIC_OFFSET = 257;
static const size_t TAR_HEADER_MAGIC_WIDTH = 6;
static const size_t TAR_HEADER_ATIME_OFFSET = 476;
static const size_t TAR_HEADER_ATIME_WIDTH = 12;
static const size_t TAR_HEADER_CTIME_OFFSET = 488;
static const size_t TAR_HEADER_CTIME_WIDTH = 12;
static const size_t TAR_HEADER_DEVMAJOR_OFFSET = 329;
static const size_t TAR_HEADER_DEVMAJOR_WIDTH = 8;
static const size_t TAR_HEADER_DEVMINOR_OFFSET = 337;
static const size_t TAR_HEADER_DEVMINOR_WIDTH = 8;
static const size_t TAR_HEADER_PREFIX_OFFSET = 345;
static const size_t TAR_HEADER_PREFIX_WIDTH = 155;
static const char   TAR_EOA_HEADER[512] = {0};
static const char  *TAR_HEADER_MAGIC_VALUE = "ustar";


typeflag_t typeflag_from_byte(const uint8_t byte)
{
    switch (byte)
    {

    case TYPEFLAG_AREG:
    case TYPEFLAG_REG:
        return TYPEFLAG_REG;
    case TYPEFLAG_LNK:
        return TYPEFLAG_LNK;
    case TYPEFLAG_SYM:
        return TYPEFLAG_SYM;
    case TYPEFLAG_CHR:
        return TYPEFLAG_CHR;
    case TYPEFLAG_BLK:
        return TYPEFLAG_BLK;
    case TYPEFLAG_DIR:
        return TYPEFLAG_DIR;
    case TYPEFLAG_FIFO:
        return TYPEFLAG_FIFO;
    case TYPEFLAG_CONT:
        return TYPEFLAG_CONT;
    case TYPEFLAG_GNUK:
        return TYPEFLAG_GNUK;
    case TYPEFLAG_GNUL:
        return TYPEFLAG_GNUL;
    case TYPEFLAG_XHD:
        return TYPEFLAG_XHD;
    case TYPEFLAG_XGL:
        return TYPEFLAG_XGL;
    case TYPEFLAG_EOA:
        /* minutar value, should not appear in tar files */
    default:
        return TYPEFLAG_UNKNOWN;
    }
}

char *parse_string_field(const char *field, size_t width)
{
    SASSERT(field != NULL);
    SASSERT(width < 255);
    char tmp[width+1];
    memcpy(tmp, field, width);
    tmp[width] = '\0';
    /* TODO: validate utf-8? */
    return strdup(field);
}

bool parse_octal_uint_field(const char *field, size_t width, size_t *output_value)
{
    SASSERT(output_value != NULL);
    SASSERT(field != NULL);
    SASSERT(width < 15 && width > 0);

    char tmp[width+1];
    memcpy(tmp, field, width);
    tmp[width] = '\0';
    long long tmp_value = strtoll(tmp, NULL, 8);

    RETURN_FALSE_IF(tmp_value < 0);
    SASSERT(tmp_value < SIZE_MAX);
    *output_value = (size_t) tmp_value;
    return true;
}

bool parse_octal_time_field(const char *field, size_t width, time_t *output_value)
{
    SASSERT(output_value != NULL);
    SASSERT(field != NULL);
    SASSERT(width < 15 && width > 0);

    char tmp[width+1];
    memcpy(tmp, field, width);
    tmp[width] = '\0';
    long long tmp_value = strtoll(tmp, NULL, 8);

    SASSERT(LLONG_MAX == LONG_MAX); /* we assume time_t is long int */
    *output_value = tmp_value;
    return true;
}

bool ustar_header_chksum_verify(const char raw_header[TAR_BLOCKSIZE])
{
    size_t expected = 0;
    RETURN_FALSE_IF(!parse_octal_uint_field(&raw_header[TAR_HEADER_CHKSUM_OFFSET], TAR_HEADER_CHKSUM_WIDTH, &expected));

    size_t calculated = 0;
    size_t i;
    for (i = 0; i < TAR_HEADER_CHKSUM_OFFSET; ++i) {
        calculated += (uint8_t)raw_header[i];
    }
    calculated += ' '*TAR_HEADER_CHKSUM_WIDTH;
    for (i = TAR_HEADER_CHKSUM_OFFSET + TAR_HEADER_CHKSUM_WIDTH; i < TAR_BLOCKSIZE; ++i) {
        calculated += (uint8_t)raw_header[i];
    }

    return (calculated == expected);
}

bool validate_mode_and_type(size_t mode, typeflag_t type)
{
    switch (type)
    {
    case TYPEFLAG_AREG:
    case TYPEFLAG_REG:
    case TYPEFLAG_CONT:
    case TYPEFLAG_LNK:
    case TYPEFLAG_SYM:
    case TYPEFLAG_DIR:
    case TYPEFLAG_CHR:
    case TYPEFLAG_BLK:
    case TYPEFLAG_FIFO:
        return (mode < 02000);

    case TYPEFLAG_GNUK:
    case TYPEFLAG_GNUL:
    case TYPEFLAG_XHD:
    case TYPEFLAG_XGL:
        /* accept whatever mode for extended header, we're not going to use it */
        return true;

    default:
        SUNREACHABLE();
    }
}


bool parse_ustar_header(const char raw_header[TAR_BLOCKSIZE], filedesc_t *output_ustar)
{
    SASSERT(output_ustar != NULL);

    /* accepts both PAX/POSIX header and GNU-style header (with ' ' instead of '\0' after magic. */
    RETURN_FALSE_IF(0 != memcmp(&raw_header[TAR_HEADER_MAGIC_OFFSET], TAR_HEADER_MAGIC_VALUE, strlen(TAR_HEADER_MAGIC_VALUE)));

    RETURN_FALSE_IF(!ustar_header_chksum_verify(raw_header));

    filedesc_t ustar;
    memset(&ustar, 0, sizeof(ustar));

    ustar.type = typeflag_from_byte(raw_header[TAR_HEADER_TYPE_OFFSET]);
    RETURN_FALSE_IF(TYPEFLAG_UNKNOWN == ustar.type);

    RETURN_FALSE_IF(!parse_octal_uint_field(&raw_header[TAR_HEADER_MODE_OFFSET], TAR_HEADER_MODE_WIDTH, &ustar.mode));
    RETURN_FALSE_IF(!validate_mode_and_type(ustar.mode, ustar.type));
    
    RETURN_FALSE_IF(!parse_octal_uint_field(&raw_header[TAR_HEADER_SIZE_OFFSET], TAR_HEADER_SIZE_WIDTH, &ustar.size));

    RETURN_FALSE_IF(!parse_octal_uint_field(&raw_header[TAR_HEADER_DEVMAJOR_OFFSET], TAR_HEADER_DEVMAJOR_WIDTH, &ustar.devmajor));
    RETURN_FALSE_IF(!parse_octal_uint_field(&raw_header[TAR_HEADER_DEVMINOR_OFFSET], TAR_HEADER_DEVMINOR_WIDTH, &ustar.devminor));

    RETURN_FALSE_IF(!parse_octal_time_field(&raw_header[TAR_HEADER_MTIME_OFFSET], TAR_HEADER_MTIME_WIDTH, &ustar.mtime));
    RETURN_FALSE_IF(!parse_octal_time_field(&raw_header[TAR_HEADER_ATIME_OFFSET], TAR_HEADER_ATIME_WIDTH, &ustar.atime));
    RETURN_FALSE_IF(!parse_octal_time_field(&raw_header[TAR_HEADER_CTIME_OFFSET], TAR_HEADER_CTIME_WIDTH, &ustar.ctime));
    
    RETURN_FALSE_IF(raw_header[TAR_HEADER_NAME_OFFSET] == '\0'); /* must have non-zero length name */
    ustar.name = parse_string_field(&raw_header[TAR_HEADER_NAME_OFFSET], TAR_HEADER_NAME_WIDTH);
    RETURN_FALSE_IF(ustar.name == NULL); /* need cleanup after this line */

    if (raw_header[TAR_HEADER_LINK_OFFSET] != '\0') {
        ustar.linktarget = parse_string_field(&raw_header[TAR_HEADER_LINK_OFFSET], TAR_HEADER_LINK_WIDTH);
        GOTO_CLEANUP_IF(NULL == ustar.linktarget);
    }

    if (raw_header[TAR_HEADER_PREFIX_OFFSET] != '\0') {
        ustar.prefix = parse_string_field(&raw_header[TAR_HEADER_PREFIX_OFFSET], TAR_HEADER_PREFIX_WIDTH);
        GOTO_CLEANUP_IF(NULL == ustar.prefix);
    }

    SASSERT(NULL != ustar.name);
    *output_ustar = ustar;
    return true;

  cleanup:
    minutar_free_filedesc(&ustar);
    return false;
}

bool read_ustar_header(FILE *tarfile, filedesc_t *output_ustar)
{
    SASSERT(tarfile != NULL);
    SASSERT(output_ustar != NULL);

    char raw_header[TAR_BLOCKSIZE];

    /* align file read pointer to TAR_BLOCKSIZE */
    long fpos = ftell(tarfile);
    RETURN_FALSE_IF( fpos < 0 );
    if (fpos % TAR_BLOCKSIZE != 0) {
        size_t skip_len = TAR_BLOCKSIZE - (fpos % TAR_BLOCKSIZE);
        RETURN_FALSE_IF(fseek(tarfile, skip_len, SEEK_CUR) != 0);
    }

    /* read the raw header data */
    RETURN_FALSE_IF(fread(raw_header, 1, sizeof(raw_header), tarfile) != TAR_BLOCKSIZE);

    /* handle end of archive condition */
    if (memcmp(raw_header, TAR_EOA_HEADER, sizeof(raw_header)) == 0) {
        RETURN_FALSE_IF(fread(raw_header, 1, sizeof(raw_header), tarfile) != TAR_BLOCKSIZE);
        RETURN_FALSE_IF(memcmp(raw_header, TAR_EOA_HEADER, sizeof(raw_header)) != 0);

        memset(output_ustar, 0, sizeof(*output_ustar));
        output_ustar->type = TYPEFLAG_EOA;
        return true;
    }

    return parse_ustar_header(raw_header, output_ustar);
}

bool read_gnulong_name(FILE *tarfile, size_t read_size, char **output_name)
{
    SASSERT(tarfile != NULL);
    SASSERT(output_name != NULL);

    char *data = NULL;

    RETURN_FALSE_IF(read_size > 0x100000);
    data = malloc(read_size+1);
    RETURN_FALSE_IF(NULL == data); /* need cleanup after this line */

    GOTO_CLEANUP_IF(fread(data, 1, read_size, tarfile) != read_size);

    data[read_size] = '\0';
    GOTO_CLEANUP_IF(strlen(data)+1 != read_size);

    *output_name = data;
    return true;

  cleanup:
    free(data);
    return false;
}

bool parse_gnulong_headers(FILE *tarfile, const filedesc_t first_header, filedesc_t *output_nextfile)
{
    SASSERT(tarfile != NULL);
    SASSERT(output_nextfile != NULL);
    SASSERT(first_header.type == TYPEFLAG_GNUK || first_header.type == TYPEFLAG_GNUL);

    char *longname = NULL;
    char *longlink = NULL;

    /* read the long name */
    if (first_header.type == TYPEFLAG_GNUL) {
        RETURN_FALSE_IF(!read_gnulong_name(tarfile, first_header.size, &longname));
    } else /* first_header.type == TYPEFLAG_GNUK */ {
        RETURN_FALSE_IF(!read_gnulong_name(tarfile, first_header.size, &longlink));
    } /* need cleanup after this line */

    /* read the next header */
    filedesc_t next_header;
    GOTO_CLEANUP_IF(!read_ustar_header(tarfile, &next_header));

    /* are both link target and name long? */
    if (next_header.type > TYPEFLAG_CONT)
    {
        /* only accept GNUL followed by GNUK and vice-versa */
        GOTO_CLEANUP_IF(next_header.type != TYPEFLAG_GNUK && next_header.type != TYPEFLAG_GNUL);
        GOTO_CLEANUP_IF(first_header.type == TYPEFLAG_GNUK && next_header.type != TYPEFLAG_GNUL);
        GOTO_CLEANUP_IF(first_header.type == TYPEFLAG_GNUL && next_header.type != TYPEFLAG_GNUK);

        /* read the next long name */
        if (next_header.type == TYPEFLAG_GNUL) {
            GOTO_CLEANUP_IF(!read_gnulong_name(tarfile, next_header.size, &longname));
        } else if (next_header.type == TYPEFLAG_GNUK) {
            GOTO_CLEANUP_IF(!read_gnulong_name(tarfile, next_header.size, &longlink));
        }

        /* read the next header */
        minutar_free_filedesc(&next_header);
        GOTO_CLEANUP_IF(!read_ustar_header(tarfile, &next_header));
    }

    /* don't accept any more extended headers */
    GOTO_CLEANUP_IF(next_header.type > TYPEFLAG_CONT);

    SASSERT(NULL != next_header.name);
    if (NULL != longname) {
        free(next_header.name);
        next_header.name = longname;
    }
    if (NULL != longlink) {
        if (NULL != next_header.linktarget) {
            free(next_header.linktarget);
        }
        next_header.linktarget = longlink;
    }
    *output_nextfile = next_header;
    return true;

  cleanup:
    if (NULL != longname) {
        free (longname);
    }
    if (NULL != longlink) {
        free (longlink);
    }
    minutar_free_filedesc(&next_header);
    return false;
}

bool extract_file_contents(FILE *tarfile, const filedesc_t file)
{
    SASSERT(tarfile != NULL);
    SASSERT(file.name != NULL);
    SASSERT(file.type != TYPEFLAG_REG || file.type != TYPEFLAG_CONT);

    FILE *output = fopen(file.name, "wb");
    RETURN_FALSE_IF(NULL == output); /* need cleanup after this line */

    GOTO_CLEANUP_IF(chmod(file.name, file.mode) != 0);

    if (file.size > 0) {
        uint8_t tmp_data[1024];
        size_t bytes_left = file.size;

        while (bytes_left > sizeof(tmp_data)) {
            GOTO_CLEANUP_IF(fread(tmp_data, sizeof(tmp_data), 1, tarfile) != 1);
            GOTO_CLEANUP_IF(fwrite(tmp_data, sizeof(tmp_data), 1, output) != 1);
            bytes_left -= sizeof(tmp_data);
        }
        GOTO_CLEANUP_IF(fread(tmp_data, 1, bytes_left, tarfile) != bytes_left);
        GOTO_CLEANUP_IF(fwrite(tmp_data, 1, bytes_left, output) != bytes_left);
    }

    fclose(output);
    return true;

  cleanup:
    fclose(output);
    return false;
}

bool extract_file(FILE *tarfile, const filedesc_t file)
{
    SASSERT(tarfile != NULL);
    SASSERT(file.name != NULL);

    switch (file.type)
    {
    case TYPEFLAG_DIR:
        RETURN_FALSE_IF(0 != mkdir(file.name, file.mode) && errno != EEXIST);
        printf("%s d\r\n", file.name);
        break;

    case TYPEFLAG_LNK:
        SASSERT(file.linktarget != NULL);
        RETURN_FALSE_IF(0 != link(file.linktarget, file.name));
        printf("%s -> %s l\r\n", file.name, file.linktarget);
        break;
    case TYPEFLAG_SYM:
        SASSERT(file.linktarget != NULL);
        RETURN_FALSE_IF(0 != symlink(file.linktarget, file.name));
        printf("%s -> %s s\r\n", file.name, file.linktarget);
        break;

    case TYPEFLAG_REG:
    case TYPEFLAG_CONT:
        RETURN_FALSE_IF(extract_file_contents(tarfile, file));
        printf("%s %lu\r\n", file.name, (unsigned long)file.size);
        break;

    case TYPEFLAG_CHR:
        RETURN_FALSE_IF(mknod(file.name, file.mode | S_IFCHR, makedev(file.devmajor, file.devminor)) != 0);
        printf("%s c\r\n", file.name);
        break;
    case TYPEFLAG_BLK:
        RETURN_FALSE_IF(mknod(file.name, file.mode | S_IFBLK, makedev(file.devmajor, file.devminor)) != 0);
        printf("%s b\r\n", file.name);
        break;
    case TYPEFLAG_FIFO:
        RETURN_FALSE_IF(mknod(file.name, file.mode | S_IFIFO, makedev(file.devmajor, file.devminor)) != 0);
        printf("%s p\r\n", file.name);
        break;

    default:
        SUNREACHABLE();
    }

    return true;
}

/********************************* PUBLIC FUNCTIONS *********************************************/

bool minutar_next_file(FILE *tarfile, filedesc_t *output_nextfile)
{
    SASSERT(tarfile != NULL);
    SASSERT(output_nextfile != NULL);

    filedesc_t nextfile;
    filedesc_t extended_header;

    RETURN_FALSE_IF(!read_ustar_header(tarfile, &nextfile)); /* need clenaup after this line */

    /* handle extended headers */
    if (nextfile.type > TYPEFLAG_CONT && nextfile.type != TYPEFLAG_EOA) {
        switch ( nextfile.type )
        {
        case TYPEFLAG_GNUL:
        case TYPEFLAG_GNUK:
            GOTO_CLEANUP_IF(!parse_gnulong_headers(tarfile, nextfile, &extended_header));
            minutar_free_filedesc(&nextfile);
            nextfile = extended_header;
            break;
    
        case TYPEFLAG_XGL:
        case TYPEFLAG_XHD:
            GOTO_CLEANUP_IF("not supported extended header");
    
        default:
            SUNREACHABLE();
        }
    }

    GOTO_CLEANUP_IF(!canonicalize_paths(&nextfile));

    SASSERT((nextfile.type >= TYPEFLAG_REG && nextfile.type <= TYPEFLAG_CONT) || nextfile.type == TYPEFLAG_EOA);
    *output_nextfile = nextfile;
    return true;

  cleanup:
    minutar_free_filedesc(&nextfile);
    return false;
}

bool minutar_skip_file(FILE *tarfile, const filedesc_t skip_file)
{
    SASSERT(tarfile != NULL);

    RETURN_FALSE_IF(fseek(tarfile, skip_file.size, SEEK_CUR) != 0);

    return true;
}

void minutar_free_filedesc(filedesc_t *file)
{
    SASSERT(file != NULL);

    if (NULL != file->name) {
        free (file->name);
        file->name = NULL;
    }
    if (NULL != file->linktarget) {
        free (file->linktarget);
        file->linktarget = NULL;
    }
    if (NULL != file->prefix) {
        free (file->prefix);
        file->prefix = NULL;
    }
}

bool minutar_extract_all(FILE *tarfile)
{
    SASSERT(tarfile != NULL);

    bool all_ok = true;
    filedesc_t next_file;

    while (minutar_next_file(tarfile, &next_file))
    {
        if (TYPEFLAG_EOA == next_file.type) {
            /* don't need to free EOA filedesc_t, since no malloc'ed content */
            break;
        }

        if (!path_mkdir(next_file.name, 0777) && errno != EEXIST) {
            fprintf(stderr, "failed to create path for '%s': %s\r\n", next_file.name, strerror(errno));
            all_ok = false;

        } else {
            if (!extract_file(tarfile, next_file)) {
                fprintf(stderr, "failed to create '%s': %s\r\n", next_file.name, strerror(errno));
                all_ok = false;
            }
        }
            
        minutar_free_filedesc(&next_file);
    }

    return all_ok;
}
