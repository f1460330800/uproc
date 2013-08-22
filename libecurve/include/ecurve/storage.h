#ifndef EC_STORAGE_H
#define EC_STORAGE_H

/** \file ecurve/storage.h
 *
 * Load and store ecurves from/to files.
 *
 * For loading an ecurve, you can use
 *      - ec_storage_load() with one of the `ec_storage_load_*()` functions
 *      - open a FILE stream yourself and use one of the `ec_storage_load_*()` functions
 *
 * And likewise for storing them (substitute "load" for "store").
 */

#include <stdio.h>
#include "ecurve/ecurve.h"

/** Values to use as the `format` argument */
enum ec_storage_format {
    EC_STORAGE_PLAIN,
    EC_STORAGE_BINARY,
    EC_STORAGE_MMAP,
};

/** Load ecurve from FILE stream
 *
 * \param ecurve    pointer to ecurve to store
 * \param stream    stream to load from
 * \param format    format to use, must be one of the values defined in #ec_storage_format
 *
 * \retval #EC_FAILURE  an error occured
 * \retval #EC_SUCCESS  else
 */
int ec_storage_load_stream(struct ec_ecurve *ecurve, FILE *stream, int format);

/** Load ecurve from a file
 *
 * Opens a file for reading, allocates a new #ec_ecurve object and parses the
 * data in the file using the given format.
 *
 * \param ecurve    pointer to ecurve to which the data will be loaded into
 * \param path      file path
 * \param format    format to use, must be one of the values defined in #ec_storage_format
 *
 * \retval #EC_FAILURE  an error occured
 * \retval #EC_SUCCESS  else
 */
int ec_storage_load_file(struct ec_ecurve *ecurve, const char *path, int format);

/** Write ecurve to FILE stream
 *
 * \param ecurve    pointer to ecurve to store
 * \param stream    stream to write to
 * \param format    format to use, must be one of the values defined in #ec_storage_format
 *
 * \retval #EC_FAILURE  an error occured
 * \retval #EC_SUCCESS  else
 */
int ec_storage_store_stream(const struct ec_ecurve *ecurve, FILE *stream, int format);

/** Store ecurve to a file
 *
 * Stores an #ec_ecurve object to a file using the given format.
 *
 * \param ecurve    pointer to ecurve to store
 * \param path      file path
 * \param format    format to use, must be one of the values defined in #ec_storage_format
 *
 * \retval #EC_FAILURE  an error occured
 * \retval #EC_SUCCESS  else
 */
int ec_storage_store_file(const struct ec_ecurve *ecurve, const char *path, int format);

#endif