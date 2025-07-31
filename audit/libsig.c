#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <link.h>
#include <sys/file.h>

static int fd = -1;
static FILE* output = 0;

unsigned int la_version(unsigned int version) {
    return LAV_CURRENT;
}

void la_preinit(uintptr_t *cookie) {
    char* outputname = getenv("LIBSIG_OUTPUT");
    if (outputname) {
        char filename[512] = { 0 };
        char* ptr = strstr(outputname, "%p");
        if (ptr != 0) {
            *(ptr + 1) = 'd';
            snprintf(filename, sizeof(filename), outputname, getpid());
        } else {
            strncpy(filename, outputname, sizeof(filename)-1);
        }

        // Try to open the file as readonly if it does exist.
        output = fopen(filename, "r");

        // If successful, just ignore it (it does not override it).
        if (output != 0) {
            fclose(output);
            output = 0;
        // Otherwise, open as writeonly to use as the output.
        } else {
            output = fopen(filename, "w");
            assert(output != 0);

            fd = fileno(output);
            assert(flock(fd, LOCK_EX) == 0);
        }
    }
}

unsigned int la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie) {
    return LA_FLG_BINDTO | LA_FLG_BINDFROM;
}

#if defined __i386__
uintptr_t la_symbind32(Elf32_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) {
    return sym->st_value;
}

Elf32_Addr la_i86_gnu_pltenter(Elf32_Sym *sym, unsigned int ndx,
        uintptr_t *refcook, uintptr_t *defcook,
        La_i86_regs *regs, unsigned int *flags,
        const char *symname, long int *framesizep) {
    if (output)
        fprintf(output, "0x%x,%s\n", sym->st_value, symname);

    return sym->st_value;
}
#elif defined __x86_64__
uintptr_t la_symbind64(Elf64_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) {
    return sym->st_value;
}

Elf64_Addr la_x86_64_gnu_pltenter(Elf64_Sym *sym, unsigned int ndx,
        uintptr_t *refcook, uintptr_t *defcook,
        La_x86_64_regs *regs, unsigned int *flags,
        const char *symname, long int *framesizep) {
    if (output)
        fprintf(output, "0x%lx,%s\n", sym->st_value, symname);

    return sym->st_value;
}
#else
#error "Unknown architecture"
#endif

__attribute__((destructor))
static void finish(void) {
    if (output) {
        fflush(output);

        assert(flock(fd, LOCK_UN) == 0);
        fclose(output);
    }
}
