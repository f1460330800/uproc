/* Compute alignment distances between amino acid words
 *
 * Copyright 2013 Peter Meinicke, Robin Martinjak
 *
 * This file is part of libuproc.
 *
 * libuproc is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * libuproc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libuproc.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uproc/common.h"
#include "uproc/error.h"
#include "uproc/alphabet.h"
#include "uproc/io.h"
#include "uproc/matrix.h"
#include "uproc/substmat.h"

#define UPROC_DISTMAT_INDEX(x, y) ((x) << UPROC_AMINO_BITS | (y))

int
uproc_substmat_init(struct uproc_substmat *mat)
{
    *mat = (struct uproc_substmat) { { { 0.0 } } };
    return 0;
}

double
uproc_substmat_get(const struct uproc_substmat *mat, unsigned pos,
                   uproc_amino x, uproc_amino y)
{
    return mat->dists[pos][UPROC_DISTMAT_INDEX(x, y)];
}

void
uproc_substmat_set(struct uproc_substmat *mat, unsigned pos, uproc_amino x,
        uproc_amino y, double dist)
{
    mat->dists[pos][UPROC_DISTMAT_INDEX(x, y)] = dist;
}

int
uproc_substmat_loadv(struct uproc_substmat *mat, enum uproc_io_type iotype,
                     const char *pathfmt, va_list ap)
{
    int res;
    size_t i, j, k, rows, cols, sz;
    struct uproc_matrix matrix;

    res = uproc_matrix_loadv(&matrix, iotype, pathfmt, ap);
    if (res) {
        return res;
    }

    uproc_matrix_dimensions(&matrix, &rows, &cols);
    sz = rows * cols;
#define SUBSTMAT_TOTAL \
    (UPROC_SUFFIX_LEN * UPROC_ALPHABET_SIZE * UPROC_ALPHABET_SIZE)
    if (sz != SUBSTMAT_TOTAL) {
        res = uproc_error_msg(
            UPROC_EINVAL,
            "invalid substitution matrix (%zu elements; expected %zu)", sz,
            SUBSTMAT_TOTAL);
        goto error;
    }

    for (i = 0; i < UPROC_SUFFIX_LEN; i++) {
        for (j = 0; j < UPROC_ALPHABET_SIZE; j++) {
            for (k = 0; k < UPROC_ALPHABET_SIZE; k++) {
                /* treat `matrix` like a vector of length SUBSTMAT_TOTAL
                 * (this assumes uproc_matrix uses a linear representation) */
                size_t idx;
                idx = (i * UPROC_ALPHABET_SIZE + j) * UPROC_ALPHABET_SIZE + k;
                uproc_substmat_set(mat, i, k, j,
                                   uproc_matrix_get(&matrix, 0, idx));
            }
        }
    }
error:
    uproc_matrix_destroy(&matrix);
    return res;
}

int
uproc_substmat_load(struct uproc_substmat *mat, enum uproc_io_type iotype,
                    const char *pathfmt, ...)
{
    int res;
    va_list ap;
    va_start(ap, pathfmt);
    res = uproc_substmat_loadv(mat, iotype, pathfmt, ap);
    va_end(ap);
    return res;
}