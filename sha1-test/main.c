#include <stdio.h>
#include <string.h>
#ifdef ORYX
#include "oryx_sha1.h"
#elif GOOGLE
#include "google_sha1.h"
#define SHA1_DIGEST_SIZE 20
#elif PD
#include "pd_sha1.h"
#elif RFC
#include "rfc_sha1.h"
#else
#include "libsha1.h"
#endif

unsigned char t1[] = "";
unsigned char t1_output[] = "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55\xbf\xef\x95\x60\x18\x90\xaf\xd8\x07\x09";

unsigned char t2[] = "1234567890";
unsigned char t2_output[] = "\x01\xb3\x07\xac\xba\x4f\x54\xf5\x5a\xaf\xc3\x3b\xb0\x6b\xbb\xf6\xca\x80\x3e\x9a";

static void print_hex(const unsigned char* data, size_t size)
{
    unsigned int i;
    printf("0x");
    for(i = 0; i < size; ++i)
        printf("%x%x", ((unsigned char)data[i])/16, ((unsigned char)data[i])%16);
}

static int num_test;

static int do_test(const unsigned char* data, size_t size, const unsigned char* expected_dgst)
{
#ifdef ORYX
	Sha1Context ctx;
#elif GOOGLE
	sha1nfo ctx;
#elif PD
    SHA1_CTX ctx;
#elif RFC
    SHA1Context ctx;
#else
    sha1_ctx ctx;
#endif
    unsigned i;
#ifdef GOOGLE
    unsigned char *res;
#else
    unsigned char dgst[SHA1_DIGEST_SIZE];
#endif

    printf("Test %d. [%d] ", num_test++, size);
    print_hex(data, size);
    printf("\nExpected        : ");
    print_hex(expected_dgst, SHA1_DIGEST_SIZE);
    printf("\n");

#ifdef ORYX
	sha1Init(&ctx);
    for(i = 0; i < size; ++i)
        sha1Update(&ctx, data+i, 1);
    sha1Final(&ctx, dgst);
#elif GOOGLE
	sha1_init(&ctx);
    for(i = 0; i < size; ++i)
        sha1_writebyte(&ctx, data[i]);
    res = sha1_result(&ctx);
#elif PD
	SHA1_Init(&ctx);
    for(i = 0; i < size; ++i)
        SHA1_Update(&ctx, data+i, 1);
    SHA1_Final(&ctx, dgst);
#elif RFC
    SHA1Reset(&ctx);
    for(i = 0; i < size; ++i)
        SHA1Input(&ctx, (uint8_t*) data+i, 1);
    SHA1Result(&ctx, dgst);
#else
    sha1_begin(&ctx);
    for(i = 0; i < size; ++i)
        sha1_hash(data+i, 1, &ctx);
    sha1_end(dgst, &ctx);
#endif

    printf("Actual (1-byte) : ");
#ifdef GOOGLE
    print_hex(res, SHA1_DIGEST_SIZE);
    if(!memcmp(res, expected_dgst, SHA1_DIGEST_SIZE))
#else
    print_hex(dgst, SHA1_DIGEST_SIZE);
    if(!memcmp(dgst, expected_dgst, SHA1_DIGEST_SIZE))
#endif
        printf(" - ok\n");
    else
    {
        printf(" - ERR\n");
        return 0;
    }

#ifdef ORYX
	sha1Init(&ctx);
    sha1Update(&ctx, data, size);
    sha1Final(&ctx, dgst);
#elif GOOGLE
	sha1_init(&ctx);
	sha1_write(&ctx, data, size);
	sha1_result(&ctx);
#elif PD
	SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, size);
    SHA1_Final(&ctx, dgst);
#elif RFC
    SHA1Reset(&ctx);
    SHA1Input(&ctx, (uint8_t*) data, size);
    SHA1Result(&ctx, dgst);
#else
    sha1_begin(&ctx);
    sha1_hash(data, size, &ctx);
    sha1_end(dgst, &ctx);
#endif
    printf("Actual (1 block): ");
#ifdef GOOGLE
    print_hex(res, SHA1_DIGEST_SIZE);
    if(!memcmp(res, expected_dgst, SHA1_DIGEST_SIZE))
#else
    print_hex(dgst, SHA1_DIGEST_SIZE);
    if(!memcmp(dgst, expected_dgst, SHA1_DIGEST_SIZE))
#endif
        printf(" - ok\n");
    else
    {
        printf(" - ERR\n");
        return 0;
    }

    return 1;
}

int main(void)
{
    do_test(t1, sizeof(t1) - 1, t1_output);
    do_test(t2, sizeof(t2) - 1, t2_output);
    return 0;
}
