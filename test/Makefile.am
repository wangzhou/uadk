AM_CFLAGS=-Wall -O0 -fno-strict-aliasing -I../include -I lib -pthread

bin_PROGRAMS=test_hisi_zip test_hisi_zlib test_dummy test_bind_api wd_zip_test

test_hisi_zip_SOURCES=test_hisi_zip.c
test_hisi_zlib_SOURCES=test_hisi_zlib.c

test_bind_api_SOURCES=test_bind_api.c test_lib.c
wd_zip_test_SOURCES=wd_zip_test.c zip_alg.c

test_hisi_zip_LDADD=../.libs/libwd.a
test_hisi_zlib_LDADD=../.libs/libwd.a
test_bind_api_LDADD=../.libs/libwd.a
wd_zip_test_LDADD=../.libs/libwd.a -lz

if HAVE_ZLIB
test_bind_api_SOURCES+=test_zlib.c
test_bind_api_LDADD+=-lz
endif

test_dummy_SOURCES=test_dummy.c

test_dummy_LDADD=../.libs/libwd.a

if WITH_OPENSSL_DIR
SUBDIRS=. hisi_hpre_test
endif
