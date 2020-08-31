#include "compiler/compiler.h"
#include "vm/env.h"
#include "vm/context.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>

#define CHECKF(x, ...)  do { if(!(x)) { fprintf(stderr, "Operation failed in %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); exit(2); } } while(0)
#define CHECK(x)        CHECKF(x, "`%s`", #x);

static char* readTextFile(const char* fname)
{
    FILE* file = fopen(fname, "rb");
    CHECKF(file, "Unable to open file `%s`: %s", fname, strerror(errno));
    CHECKF(!fseek(file, 0, SEEK_END), "Unable to seek to end of file `%s`: %s", fname, strerror(errno));
    long int flen = ftell(file);
    CHECKF(flen >= 0, "Unable to get length of file `%s`: %s", fname, strerror(errno));
    CHECKF(!fseek(file, 0, SEEK_SET), "Unable to seek to start of file `%s`: %s", fname, strerror(errno));
    char* buf = malloc(flen + 1);
    CHECKF(buf, "Unable to allocate memory for file `%s`", fname);
    CHECKF(fread(buf, 1, flen, file) == flen, "Unable to read file `%s`: %s\n", fname, strerror(errno));
    buf[flen] = 0;
    fclose(file);
    return buf;
}
static void usage(const char* argv0, int exit_code)
{
    FILE* stream = exit_code ? stderr : stdout;
    fprintf(stream, "Usage: %s <file>\n", argv0 ? argv0 : "coyote");
    exit(exit_code);
}
int main(int argc, char** argv)
{
    if(argc < 2) usage(argv[0], 1);
    char* src = readTextFile(argv[1]);

    // an environment is essentially a "registry" of all static data: modules, function code, type information, ...
    coy_env_t env;
    CHECK(coy_env_init(&env));

    // a compiler is just that; note that the context does *not* need to be preserved after code is compiled
    coyc_t compiler;
    CHECK(coyc_init(&compiler, &env));
    if(!coyc_compile(&compiler, argv[1], src))
        exit(3);
    CHECK(coyc_deinit(&compiler));

    // a context holds the runtime information (program state)
    coy_context_t* ctx = coy_context_create(&env);

    // slots[0] = main.main()
    coy_ensure_slots(ctx, 0);
    coy_call(ctx, "main", "main");
    int32_t i = coy_get_uint(ctx, 0);
    printf("main.main() returned: %" PRId32 "\n", i);

    coy_env_deinit(&env);
    free(src);
    return 0;
}
