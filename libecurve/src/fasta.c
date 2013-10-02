#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <assert.h>

#include "ecurve/common.h"
#include "ecurve/io.h"
#include "ecurve/fasta.h"

void
ec_fasta_reader_init(struct ec_fasta_reader *rd, size_t seq_sz_hint)
{
    static struct ec_fasta_reader zero;
    *rd = zero;
    rd->seq_sz_hint = seq_sz_hint;
}

void
ec_fasta_reader_free(struct ec_fasta_reader *rd)
{
    free(rd->line);
    free(rd->header);
    free(rd->seq);
}

static void
reader_getline(ec_io_stream *stream, struct ec_fasta_reader *rd)
{
    rd->line_len = ec_io_getline(&rd->line, &rd->line_sz, stream);
    rd->line_no++;
}

int
ec_fasta_read(ec_io_stream *stream, struct ec_fasta_reader *rd)
{
    size_t len, total_len;
    if (!rd->header) {
        rd->header_sz = 0;
    }
    if (!rd->comment) {
        rd->comment_sz = 0;
    }
    if (!rd->seq) {
        if (rd->seq_sz_hint) {
            rd->seq = malloc(rd->seq_sz_hint);
            if (!rd->seq) {
                return EC_ENOMEM;
            }
        }
        rd->seq_sz = rd->seq_sz_hint;
    }

    if (!rd->line) {
        reader_getline(stream, rd);
    }

    if (rd->line_len == -1) {
        return EC_ITER_STOP;
    }

    /* header needs at least '>', one character, '\n' */
    if (rd->line_len < 3 || rd->line[0] != '>') {
        return EC_EINVAL;
    }

    len = rd->line_len - 1;
    if (rd->header_sz < len) {
        void *tmp = realloc(rd->header, len);
        if (!tmp) {
            return EC_ENOMEM;
        }
        rd->header = tmp;
        rd->header_sz = len;
    }
    memcpy(rd->header, rd->line + 1, len - 1);
    rd->header[len - 1] = '\0';
    rd->header_len = len - 1;
    assert(rd->header_len == strlen(rd->header));

    reader_getline(stream, rd);
    if (rd->line_len == -1) {
        return EC_EINVAL;
    }

    for (total_len = 0;
         rd->line[0] == ';' && rd->line_len != -1;
         reader_getline(stream, rd))
    {
        len = rd->line_len;
        if (rd->comment_sz < total_len + len) {
            void *tmp = realloc(rd->comment, total_len + len);
            if (!tmp) {
                return EC_ENOMEM;
            }
            rd->comment = tmp;
            rd->comment_sz = total_len + len;
        }
        /* skip leading ';' but keep newline */
        memcpy(rd->comment + total_len, rd->line + 1, len - 1);
        total_len += len - 1;
    }
    if (total_len > 1) {
        /* remove last newline */
        rd->comment[total_len - 1] = '\0';
    }
    rd->comment_len = total_len - 1;
    assert(!rd->comment || rd->comment_len == strlen(rd->comment));

    for (total_len = 0;
         rd->line[0] != '>' && rd->line_len != -1;
         reader_getline(stream, rd))
    {
        len = rd->line_len;
        if (rd->seq_sz < total_len + len) {
            void *tmp = realloc(rd->seq, total_len + len);
            if (!tmp) {
                return EC_ENOMEM;
            }
            rd->seq = tmp;
            rd->seq_sz = total_len + len;
        }
        memcpy(rd->seq + total_len, rd->line, len - 1);
        total_len += len - 1;
        rd->seq[total_len] = '\0';
    }
    rd->seq_len = total_len;
    assert(rd->seq_len == strlen(rd->seq));

    return EC_ITER_YIELD;
}

void
ec_fasta_write(ec_io_stream *stream, const char *id, const char *comment,
                 const char *seq, unsigned width)
{
    ec_io_printf(stream, ">%s\n", id);

    if (comment && *comment) {
        const char *c = comment, *nl;
        while (*c) {
            nl = strchr(c, '\n');
            if (!nl) {
                nl = c + strlen(c);
            }
            ec_io_printf(stream, ";%.*s\n", (int)(nl - c), c);
            c = nl + !!*nl;
        }
    }

    if (!width) {
        ec_io_printf(stream, "%s\n", seq);
        return;
    }

    while (*seq) {
        seq += ec_io_printf(stream, "%.*s\n", width, seq) - 1;
    }
}
