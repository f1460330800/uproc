if HAVE_CHECK
SUBDIRS = data
TESTS = ck_common \
		ck_alphabet \
		ck_bst \
		ck_codon \
		ck_idmap \
		ck_list \
		ck_matrix \
		ck_word

check_PROGRAMS = $(TESTS)

AM_CFLAGS = @CHECK_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/libuproc/include \
				-DDATADIR=\"$(abs_top_srcdir)/libuproc/tests/data/\" \
				-DTMPDATADIR=\"$(abs_top_builddir)/libuproc/tests/data/\"
LDADD = $(top_builddir)/libuproc/libuproc.la @CHECK_LIBS@
endif
