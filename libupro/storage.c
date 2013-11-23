#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>

#include "upro/common.h"
#include "upro/error.h"
#include "upro/ecurve.h"
#include "upro/io.h"
#include "upro/mmap.h"
#include "upro/word.h"
#include "upro/storage.h"


#define STR1(x) #x
#define STR(x) STR1(x)
#define BUFSZ 1024
#define COMMENT_CHAR '#'
#define HEADER_PRI ">> alphabet: %." STR(UPRO_ALPHABET_SIZE) "s, suffixes: %zu\n"
#define HEADER_SCN ">> alphabet: %"  STR(UPRO_ALPHABET_SIZE) "c, suffixes: %zu"

#define PREFIX_PRI ">%." STR(UPRO_PREFIX_LEN) "s %zu\n"
#define PREFIX_SCN ">%"  STR(UPRO_PREFIX_LEN) "c %zu"

#define SUFFIX_PRI "%." STR(UPRO_SUFFIX_LEN) "s %" UPRO_FAMILY_PRI "\n"
#define SUFFIX_SCN "%"  STR(UPRO_SUFFIX_LEN) "c %" UPRO_FAMILY_SCN

static int
read_line(upro_io_stream *stream, char *line, size_t n)
{
    do {
        if (!upro_io_gets(line, n, stream)) {
            return -1;
        }
    } while (line[0] == COMMENT_CHAR);
    return 0;
}

static int
load_header(upro_io_stream *stream, char *alpha, size_t *suffix_count)
{
    char line[BUFSZ];
    int res = read_line(stream, line, sizeof line);
    if (res) {
        return res;
    }
    res = sscanf(line, HEADER_SCN, alpha, suffix_count);
    alpha[UPRO_ALPHABET_SIZE] = '\0';
    if (res != 2) {
        return upro_error_msg(UPRO_EINVAL, "invalid header: \"%s\"", line);
    }
    return 0;
}

static int
load_prefix(upro_io_stream *stream, const struct upro_alphabet *alpha,
                  upro_prefix *prefix, size_t *suffix_count)
{
    int res;
    char line[BUFSZ], word_str[UPRO_WORD_LEN + 1];
    struct upro_word word = UPRO_WORD_INITIALIZER;

    res = read_line(stream, line, sizeof line);
    if (res) {
        return res;
    }
    res = upro_word_to_string(word_str, &word, alpha);
    if (res) {
        return res;
    }
    res = sscanf(line, PREFIX_SCN, word_str, suffix_count);
    if (res != 2) {
        return upro_error_msg(UPRO_EINVAL, "invalid prefix string: \"%s\"",
                              line);
    }
    res = upro_word_from_string(&word, word_str, alpha);
    if (res) {
        return res;
    }
    *prefix = word.prefix;
    return 0;
}

static int
load_suffix(upro_io_stream *stream, const struct upro_alphabet *alpha,
                  upro_suffix *suffix, upro_family *family)
{
    int res;
    char line[BUFSZ], word_str[UPRO_WORD_LEN + 1];
    struct upro_word word = UPRO_WORD_INITIALIZER;

    res = read_line(stream, line, sizeof line);
    if (res) {
        return res;
    }
    res = upro_word_to_string(word_str, &word, alpha);
    if (res) {
        return res;
    }
    res = sscanf(line, SUFFIX_SCN, &word_str[UPRO_PREFIX_LEN], family);
    if (res != 2) {
        return upro_error_msg(UPRO_EINVAL, "invalid suffix string: \"%s\"",
                              line);
    }
    res = upro_word_from_string(&word, word_str, alpha);
    if (res) {
        return res;
    }
    *suffix = word.suffix;
    return 0;
}

static int
upro_storage_plain_load(struct upro_ecurve *ecurve, upro_io_stream *stream)
{
    int res;
    upro_prefix p;
    upro_suffix s;
    size_t suffix_count, prev_last;
    char alpha[UPRO_ALPHABET_SIZE + 1];
    res = load_header(stream, alpha, &suffix_count);

    if (res) {
        return res;
    }

    res = upro_ecurve_init(ecurve, alpha, suffix_count);
    if (res) {
        return res;
    }

    for (prev_last = 0, s = p = 0; s < suffix_count;) {
        upro_prefix prefix = -1;
        size_t ps, p_suffixes;

        res = load_prefix(stream, &ecurve->alphabet, &prefix, &p_suffixes);
        if (res) {
            return res;
        }

        if (prefix > UPRO_PREFIX_MAX) {
            return upro_error_msg(UPRO_EINVAL,
                                  "invalid prefix value: %" UPRO_PREFIX_PRI,
                                  prefix);
        }

        for (; p < prefix; p++) {
            ecurve->prefixes[p].first = prev_last;
            ecurve->prefixes[p].count = prev_last ? 0 : UPRO_ECURVE_EDGE;
        }
        ecurve->prefixes[prefix].first = s;
        ecurve->prefixes[prefix].count = p_suffixes;
        p++;

        for (ps = 0; ps < p_suffixes; ps++, s++) {
            res = load_suffix(stream, &ecurve->alphabet,
                    &ecurve->suffixes[s], &ecurve->families[s]);
            if (res) {
                return res;
            }
        }
        prev_last = s - 1;
    }
    for (; p < UPRO_PREFIX_MAX + 1; p++) {
        ecurve->prefixes[p].first = prev_last;
        ecurve->prefixes[p].count = UPRO_ECURVE_EDGE;
    }
    return 0;
}

static int
store_header(upro_io_stream *stream, const char *alpha, size_t suffix_count)
{
    int res;
    res = upro_io_printf(stream, HEADER_PRI, alpha, suffix_count);
    return (res > 0) ? 0 : -1;
}

static int
store_prefix(upro_io_stream *stream, const struct upro_alphabet *alpha,
        upro_prefix prefix, size_t suffix_count)
{
    int res;
    char str[UPRO_WORD_LEN + 1];
    struct upro_word word = UPRO_WORD_INITIALIZER;
    word.prefix = prefix;
    res = upro_word_to_string(str, &word, alpha);
    if (res) {
        return res;
    }
    str[UPRO_PREFIX_LEN] = '\0';
    res = upro_io_printf(stream, PREFIX_PRI, str, suffix_count);
    return (res > 0) ? 0 : -1;
}

static int
store_suffix(upro_io_stream *stream, const struct upro_alphabet *alpha,
        upro_suffix suffix, upro_family family)
{
    int res;
    char str[UPRO_WORD_LEN + 1];
    struct upro_word word = UPRO_WORD_INITIALIZER;
    word.suffix = suffix;
    res = upro_word_to_string(str, &word, alpha);
    if (res) {
        return res;
    }
    res = upro_io_printf(stream, SUFFIX_PRI, &str[UPRO_PREFIX_LEN], family);
    return (res > 0) ? 0 : -1;
}

static int
upro_storage_plain_store(const struct upro_ecurve *ecurve, upro_io_stream *stream)
{
    int res;
    size_t p;

    res = store_header(stream, ecurve->alphabet.str, ecurve->suffix_count);

    if (res) {
        return res;
    }

    for (p = 0; p <= UPRO_PREFIX_MAX; p++) {
        size_t suffix_count = ecurve->prefixes[p].count;
        size_t offset = ecurve->prefixes[p].first;
        size_t i;

        if (!suffix_count || UPRO_ECURVE_ISEDGE(ecurve->prefixes[p])) {
            continue;
        }

        res = store_prefix(stream, &ecurve->alphabet, p, suffix_count);
        if (res) {
            return res;
        }

        for (i = 0; i < suffix_count; i++) {
            res = store_suffix(stream, &ecurve->alphabet,
                    ecurve->suffixes[offset + i], ecurve->families[offset + i]);
            if (res) {
                return res;
            }
        }
    }
    return 0;
}


static int
upro_storage_binary_load(struct upro_ecurve *ecurve, upro_io_stream *stream)
{
    int res;
    size_t sz;
    size_t suffix_count;
    char alpha[UPRO_ALPHABET_SIZE + 1];

    sz = upro_io_read(alpha, sizeof *alpha, UPRO_ALPHABET_SIZE, stream);
    if (sz != UPRO_ALPHABET_SIZE) {
        return upro_error(UPRO_ERRNO);
    }
    alpha[UPRO_ALPHABET_SIZE] = '\0';

    sz = upro_io_read(&suffix_count, sizeof suffix_count, 1, stream);
    if (sz != 1) {
        return upro_error(UPRO_ERRNO);
    }

    res = upro_ecurve_init(ecurve, alpha, suffix_count);
    if (res) {
        return res;
    }

    sz = upro_io_read(ecurve->suffixes, sizeof *ecurve->suffixes, suffix_count,
            stream);
    if (sz != suffix_count) {
        res = upro_error(UPRO_ERRNO);
        goto error;
    }
    sz = upro_io_read(ecurve->families, sizeof *ecurve->families, suffix_count,
            stream);
    if (sz != suffix_count) {
        res = upro_error(UPRO_ERRNO);
        goto error;
    }

    for (upro_prefix i = 0; i <= UPRO_PREFIX_MAX; i++) {
        sz = upro_io_read(&ecurve->prefixes[i].first,
                sizeof ecurve->prefixes[i].first, 1, stream);
        if (sz != 1) {
            res = upro_error(UPRO_ERRNO);
            goto error;
        }
        sz = upro_io_read(&ecurve->prefixes[i].count,
                sizeof ecurve->prefixes[i].count, 1, stream);
        if (sz != 1) {
            res = upro_error(UPRO_ERRNO);
            goto error;
        }
    }

    return 0;
error:
    upro_ecurve_destroy(ecurve);
    return res;
}

static int
upro_storage_binary_store(const struct upro_ecurve *ecurve, upro_io_stream *stream)
{
    int res;
    size_t sz;
    struct upro_alphabet alpha;

    upro_ecurve_get_alphabet(ecurve, &alpha);
    sz = upro_io_write(alpha.str, 1, UPRO_ALPHABET_SIZE, stream);
    if (sz != UPRO_ALPHABET_SIZE) {
        return upro_error(UPRO_ERRNO);
    }

    sz = upro_io_write(&ecurve->suffix_count, sizeof ecurve->suffix_count, 1,
            stream);
    if (sz != 1) {
        return upro_error(UPRO_ERRNO);
    }

    sz = upro_io_write(ecurve->suffixes, sizeof *ecurve->suffixes,
            ecurve->suffix_count, stream);
    if (sz != ecurve->suffix_count) {
        res = UPRO_ERRNO;
        goto error;
    }
    sz = upro_io_write(ecurve->families, sizeof *ecurve->families,
            ecurve->suffix_count, stream);
    if (sz != ecurve->suffix_count) {
        res = upro_error(UPRO_ERRNO);
        goto error;
    }

    for (upro_prefix i = 0; i <= UPRO_PREFIX_MAX; i++) {
        sz = upro_io_write(&ecurve->prefixes[i].first,
                sizeof ecurve->prefixes[i].first, 1, stream);
        if (sz != 1) {
            res = upro_error(UPRO_ERRNO);
            goto error;
        }
        sz = upro_io_write(&ecurve->prefixes[i].count,
                sizeof ecurve->prefixes[i].count, 1, stream);
        if (sz != 1) {
            res = upro_error(UPRO_ERRNO);
            goto error;
        }
    }

    return 0;
error:
    return res;
}

int
upro_storage_loadv(struct upro_ecurve *ecurve,
        enum upro_storage_format format, enum upro_io_type iotype,
        const char *pathfmt, va_list ap)
{
    int res;
    va_list aq;
    upro_io_stream *stream;
    const char *mode[] = {
        [UPRO_STORAGE_BINARY] = "rb",
        [UPRO_STORAGE_PLAIN] = "r",
    };

    va_copy(aq, ap);

    if (format == UPRO_STORAGE_MMAP) {
        res = upro_mmap_mapv(ecurve, pathfmt, aq);
        va_end(aq);
        return res;
    }

    stream = upro_io_openv(mode[format], iotype, pathfmt, aq);
    va_end(aq);
    if (!stream) {
        return -1;
    }
    if (format == UPRO_STORAGE_BINARY) {
        res = upro_storage_binary_load(ecurve, stream);
    }
    else {
        res = upro_storage_plain_load(ecurve, stream);
    }
    upro_io_close(stream);
    return res;
}
int
upro_storage_load(struct upro_ecurve *ecurve,
        enum upro_storage_format format, enum upro_io_type iotype,
        const char *pathfmt, ...)
{
    int res;
    va_list ap;
    va_start(ap, pathfmt);
    res = upro_storage_loadv(ecurve, format, iotype, pathfmt, ap);
    va_end(ap);
    return res;
}

int
upro_storage_storev(const struct upro_ecurve *ecurve,
        enum upro_storage_format format, enum upro_io_type iotype,
        const char *pathfmt, va_list ap)
{
    int res;
    va_list aq;
    upro_io_stream *stream;
    static const char *mode[] = {
        [UPRO_STORAGE_BINARY] = "wb",
        [UPRO_STORAGE_PLAIN] = "w",
    };

    va_copy(aq, ap);

    if (format == UPRO_STORAGE_MMAP) {
        res = upro_mmap_storev(ecurve, pathfmt, aq);
        va_end(aq);
        return res;
    }

    stream = upro_io_openv(mode[format], iotype, pathfmt, aq);
    if (!stream) {
        return -1;
    }
    if (format == UPRO_STORAGE_BINARY) {
        res = upro_storage_binary_store(ecurve, stream);
    }
    else {
        res = upro_storage_plain_store(ecurve, stream);
    }
    upro_io_close(stream);
    return res;
}

int
upro_storage_store(const struct upro_ecurve *ecurve,
        enum upro_storage_format format, enum upro_io_type iotype,
        const char *pathfmt, ...)
{
    int res;
    va_list ap;
    va_start(ap, pathfmt);
    res = upro_storage_storev(ecurve, format, iotype, pathfmt, ap);
    va_end(ap);
    return res;
}