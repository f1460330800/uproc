#include <stdlib.h>
#include <errno.h>
#include "check_workaround.h"
#include "uproc.h"

uproc_idmap *map;

void setup(void)
{
    map = uproc_idmap_create();
}

void teardown(void)
{
    if (map) {
        uproc_idmap_destroy(map);
    }
}

START_TEST(test_usage)
{
    uproc_class fam1, fam2;

    fam1 = uproc_idmap_class(map, "foo");
    fam2 = uproc_idmap_class(map, "foo");
    ck_assert_int_eq(fam1, fam2);
    ck_assert_str_eq(uproc_idmap_str(map, fam1), "foo");

    fam2 = uproc_idmap_class(map, "bar");
    ck_assert_int_ne(fam1, fam2);
    ck_assert_str_eq(uproc_idmap_str(map, fam2), "bar");

    fam2 = uproc_idmap_class(map, "herp derp");
    ck_assert_int_ne(fam1, fam2);
    ck_assert_str_eq(uproc_idmap_str(map, fam2), "herp derp");

    fam1 = uproc_idmap_class(map, "herp");
    ck_assert_int_ne(fam1, fam2);
    ck_assert_str_eq(uproc_idmap_str(map, fam1), "herp");

    fam1 = uproc_idmap_class(map, "bar");
    ck_assert_int_ne(fam1, fam2);
    ck_assert_str_eq(uproc_idmap_str(map, fam1), "bar");

    fam2 = uproc_idmap_class(map, "bar");
    ck_assert_int_eq(fam1, fam2);
    ck_assert_str_eq(uproc_idmap_str(map, fam2), "bar");
    ck_assert_str_eq(uproc_idmap_str(map, fam1),
                     uproc_idmap_str(map, fam2));
}
END_TEST

START_TEST(test_store_load)
{
    int res;
    uproc_class fam;

    uproc_idmap_class(map, "foo");
    uproc_idmap_class(map, "bar");
    uproc_idmap_class(map, "baz");
    uproc_idmap_class(map, "quux");
    uproc_idmap_class(map, "42");
    uproc_idmap_class(map, "herp derp");
    res = uproc_idmap_store(map, UPROC_IO_GZIP, TMPDATADIR "test_idmap.tmp");
    ck_assert_msg(res == 0, "storing idmap failed");

    uproc_idmap_destroy(map);
    map = uproc_idmap_load(UPROC_IO_GZIP, TMPDATADIR "test_idmap.tmp");
    ck_assert_ptr_ne(map, NULL);

    fam = uproc_idmap_class(map, "bar");
    ck_assert_int_eq(fam, 1);

    fam = uproc_idmap_class(map, "quux");
    ck_assert_int_eq(fam, 3);

    fam = uproc_idmap_class(map, "derp");
    ck_assert_int_eq(fam, 6);
}
END_TEST

START_TEST(test_load_invalid)
{
    uproc_idmap_destroy(map);
    map = uproc_idmap_load(UPROC_IO_GZIP, DATADIR "no_such_file");
    ck_assert_ptr_eq(map, NULL);
    ck_assert_int_eq(uproc_errno, UPROC_ERRNO);
    ck_assert_int_eq(errno, ENOENT);

    map = uproc_idmap_load(UPROC_IO_GZIP, DATADIR "invalid_header.idmap");
    ck_assert_ptr_eq(map, NULL);
    ck_assert_int_eq(uproc_errno, UPROC_EINVAL);

    map = uproc_idmap_load(UPROC_IO_GZIP, DATADIR "duplicate.idmap");
    ck_assert_ptr_eq(map, NULL);
    ck_assert_int_eq(uproc_errno, UPROC_EINVAL);

    map = uproc_idmap_load(UPROC_IO_GZIP, DATADIR "missing_entry.idmap");
    ck_assert_ptr_eq(map, NULL);
    ck_assert_int_eq(uproc_errno, UPROC_EINVAL);
}
END_TEST

int main(void)
{
    Suite *s = suite_create("idmap");
    TCase *tc = tcase_create("idmap operations");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_usage);
    tcase_add_test(tc, test_store_load);
    tcase_add_test(tc, test_load_invalid);
    suite_add_tcase(s, tc);

    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int n_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return n_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
