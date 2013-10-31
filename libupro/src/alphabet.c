#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "upro/common.h"
#include "upro/alphabet.h"

int
upro_alphabet_init(struct upro_alphabet *alpha, const char *s)
{
    unsigned char i;
    const char *p;

    if (strlen(s) != UPRO_ALPHABET_SIZE) {
        return UPRO_EINVAL;
    }
    strcpy(alpha->str, s);
    for (i = 0; i < UCHAR_MAX; i++) {
        alpha->aminos[i] = -1;
    }
    for (p = s; *p; p++) {
        i = *p;
        /* invalid or duplicate character */
        if (!isupper(i) || alpha->aminos[i] != -1) {
            return UPRO_EINVAL;
        }
        alpha->aminos[i] = p - s;
    }
    return UPRO_SUCCESS;
}

upro_amino
upro_alphabet_char_to_amino(const struct upro_alphabet *alpha, int c)
{
    return alpha->aminos[(unsigned char)c];
}

int
upro_alphabet_amino_to_char(const struct upro_alphabet *alpha, upro_amino amino)
{
    if (amino >= UPRO_ALPHABET_SIZE) {
        return -1;
    }
    return alpha->str[amino];
}