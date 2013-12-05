#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#if _OPENMP
#include <omp.h>
#endif

#include <uproc.h>
#include "makedb.h"

#define SEQ_COUNT_MULTIPLIER 10000
#define POW_MIN 5
#define POW_MAX 11
#define POW_DIFF (POW_MAX - POW_MIN)
#define LEN_MAX (1 << POW_MAX)

#define INTERP_MIN 20
#define INTERP_MAX 5000

#define OUT_PREFIX_DEFAULT "prot_thresh_e"

#define ELEMENTS_OF(x) (sizeof (x) / sizeof (x)[0])

static void *
xrealloc(void *ptr, size_t sz)
{
    ptr = realloc(ptr, sz);
    if (!ptr) {
        fprintf(stderr, "memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}
#define xmalloc(sz) xrealloc(NULL, sz)

static int
choice(const struct uproc_matrix *p, size_t n)
{
    double sum, c;
    unsigned i;
    c = (double)rand() / RAND_MAX;
    for (sum = 0, i = 0; sum < c && i < n; ++i) {
        if (p) {
            sum += uproc_matrix_get(p, 0, i);
        }
        else {
            sum += 1.0 / n;
        }
    }
    return i - 1;
}

static void
randseq(char *buf, size_t len, const struct uproc_alphabet *alpha,
        const struct uproc_matrix *probs)
{
    size_t i;
    static bool rand_seeded = false;
    if (!rand_seeded) {
        srand(time(NULL));
        rand_seeded = true;
    }
    for (i = 0; i < len; i++) {
        uproc_amino a = choice(probs, UPROC_ALPHABET_SIZE);
        buf[i] = uproc_alphabet_amino_to_char(alpha, a);
    }
    buf[i] = '\0';
}

static void
append(double **dest, size_t *dest_n, size_t *dest_sz,
       struct uproc_pc_pred *src, size_t n)
{
    double *d = *dest;
    if (!d) {
        *dest_sz = 1000000;
        d = xrealloc(d, *dest_sz * sizeof *d);
    }
    if (*dest_n + n > *dest_sz) {
        while (*dest_n + n > *dest_sz) {
            *dest_sz += 100000;
        }
        d = xrealloc(d, *dest_sz * sizeof *d);
    }
    for (size_t i = 0; i < n; i++) {
        d[*dest_n + i] = src[i].score;
    }
    *dest = d;
    *dest_n += n;
}

static int
double_cmp(const void *p1, const void *p2)
{
    double delta = *(const double *)p1 - *(const double*)p2;
    if (fabs(delta) < UPROC_EPSILON) {
        return 0;
    }
    /* we want to sort in descending order */
    return delta < 0.0 ? 1 : -1;
}


static int
csinterp(const double *xa, const double *ya, int m, const double *x, double *y,
         int n)
{
    int i, low, high, mid;
    double h, b, a, u[m], ya2[m];

    ya2[0] = ya2[m-1] = u[0] = 0.0;

    for (i = 1; i < m - 1; i++) {
        a = (xa[i] - xa[i-1]) / (xa[i+1] - xa[i-1]);
        b = a * ya2[i-1] + 2.0;
        ya2[i] = (a - 1.0) / b;
        u[i] = (ya[i+1] - ya[i]) / (xa[i+1] - xa[i]) - (ya[i] - ya[i-1]) /
            (xa[i] - xa[i-1]);
        u[i] = (6.0 * u[i] / (xa[i+1] - xa[i-1]) - a * u[i-1]) / b;
    }

    for (i = m - 1; i > 0; i--) {
        ya2[i-1] *= ya2[i];
        ya2[i-1] += u[i-1];
    }

    low = 0;
    high = m - 1;

    for (i = 0; i < n; i++) {
        if (i && (xa[low] > x[i] || xa[high] < x[i])) {
            low = 0;
            high = m - 1;
        }

        while (high - low > 1) {
            mid = (high + low) / 2;
            if (xa[mid] > x[i]) {
                high = mid;
            }
            else {
                low = mid;
            }
        }

        h = xa[high] - xa[low];
        if (h == 0.0) {
            return UPROC_EINVAL;
        }

        a = (xa[high] - x[i]) / h;
        b = (x[i] - xa[low]) / h;
        y[i] = a * ya[low]
            + b * ya[high]
            + ((a*a*a - a) * ya2[low] + (b*b*b - b) * ya2[high])
            * (h*h) / 6.0;
    }
    return 0;
}

static int
store_interpolated(double thresh[static POW_DIFF + 1],
                   const char *dbdir, const char *name)
{
    size_t i;
    double xa[POW_DIFF + 1],
           x[INTERP_MAX], y[INTERP_MAX];

    struct uproc_matrix thresh_interp = {
        .rows = 1, .cols = INTERP_MAX, .values = y };

    for (i = 0; i < ELEMENTS_OF(xa); i++) {
        xa[i] = i;
    }

    for (i = 0; i < ELEMENTS_OF(x); i++) {
        double xi = i;
        if (i < INTERP_MIN) {
            xi = INTERP_MIN;
        }
        x[i] = log2(xi) - POW_MIN;
    }

    csinterp(xa, thresh, POW_DIFF + 1, x, y, INTERP_MAX);
    return uproc_matrix_store(&thresh_interp, UPROC_IO_STDIO, "%s/%s", dbdir,
                              name);
}

static bool
prot_filter(const char *seq, size_t len, uproc_family family, double score,
            void *opaque)
{
    (void) seq;
    (void) len;
    (void) family;
    (void) opaque;
    return true;
    return score > 0.0;
}

int calib(const char *alphabet, const char *dbdir, const char *modeldir)
{
    int res;
    struct uproc_alphabet alpha;
    struct uproc_matrix aa_probs;
    struct uproc_substmat substmat;
    struct uproc_ecurve fwd, rev;
    double thresh2[POW_DIFF + 1],
           thresh3[POW_DIFF + 1];

    res = uproc_substmat_load(&substmat, UPROC_IO_GZIP, "%s/substmat",
                              modeldir);
    if (res) {
        uproc_perror("error loading substmat");
        return res;
    }

    /* the string used to initialize alpha must match the one used to build
     * aa_probs
     */
    res = uproc_alphabet_init(&alpha, alphabet);
    if (res) {
        uproc_perror("invalid alphabet string");
        return res;
    }

    res = uproc_matrix_load(&aa_probs, UPROC_IO_GZIP, "%s/aa_probs", modeldir);
    if (res) {
        uproc_perror("error loading aa_probs");
        return res;
    }

    res = uproc_storage_load(&fwd, UPROC_STORAGE_BINARY, UPROC_IO_GZIP,
                             "%s/fwd.ecurve", dbdir);
    if (res) {
        uproc_perror("error opening forward ecurve");
        return res;
    }
    res = uproc_storage_load(&rev, UPROC_STORAGE_BINARY, UPROC_IO_GZIP,
                             "%s/rev.ecurve", dbdir);
    if (res) {
        uproc_perror("error opening reverse ecurve");
        return res;
    }

#if _OPENMP
    omp_set_num_threads(POW_MAX - POW_MIN + 1);
#endif
    double perc = 0.0;
    progress("calibrating", perc);
#pragma omp parallel private(res) shared(fwd, rev, substmat, alpha, aa_probs, perc)
    {

#pragma omp for
        for (unsigned power = POW_MIN; power <= POW_MAX; power++) {
            char seq[LEN_MAX + 1];

            size_t all_preds_n = 0, all_preds_sz = 0;
            double *all_preds = NULL;

            unsigned long i, seq_count;
            size_t seq_len;
            struct uproc_protclass pc;
            struct uproc_pc_results results;
            results.preds = NULL;
            results.n = results.sz = 0;
            uproc_pc_init(&pc, UPROC_PC_ALL, &fwd, &rev, &substmat,
                          prot_filter, NULL);

            all_preds_n = 0;
            seq_len = 1 << power;
            seq_count = (1 << (POW_MAX - power)) * SEQ_COUNT_MULTIPLIER;
            seq[seq_len] = '\0';
            for (i = 0; i < seq_count; i++) {
                randseq(seq, seq_len, &alpha, &aa_probs);
                uproc_pc_classify(&pc, seq, &results);
                append(&all_preds, &all_preds_n, &all_preds_sz, results.preds,
                        results.n);
            }
            qsort(all_preds, all_preds_n, sizeof *all_preds, &double_cmp);

#define MIN(a, b) ((a) < (b) ? (a) : (b))
            thresh2[power - POW_MIN] = all_preds[MIN(seq_count / 100,
                    all_preds_n - 1)];
            thresh3[power - POW_MIN] = all_preds[MIN(seq_count / 1000,
                    all_preds_n - 1)];
            free(all_preds);
            free(results.preds);
#pragma omp critical
            {
                perc += 100.0 / (POW_MAX - POW_MIN + 1);
                progress("calibrating", perc);
            }
        }
    }

    res = store_interpolated(thresh2, dbdir, "prot_thresh_e2");
    if (res) {
        return res;
    }
    res = store_interpolated(thresh3, dbdir, "prot_thresh_e3");
    return res;
}