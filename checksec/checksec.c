#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void check_RELRO(void *elf, Elf64_Ehdr *ehdr, Elf64_Phdr *phdr, Elf64_Shdr *shdr) {
    int has_relro    = 0;
    int has_bind_now = 0;
    uint64_t dyn_off = 0;
    size_t   dyn_sz  = 0;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_GNU_RELRO) {
            has_relro = 1;
            break;
        }
    }

    Elf64_Shdr *shstr_hdr = &shdr[ehdr->e_shstrndx];
    const char *shstrtab = (const char *)elf + shstr_hdr->sh_offset;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *name = shstrtab + shdr[i].sh_name;
        if (strcmp(name, ".dynamic") == 0) {
            dyn_off = shdr[i].sh_offset;
            dyn_sz = shdr[i].sh_size;
            break;
        }
    }

    if (dyn_off && dyn_sz) {
        Elf64_Dyn *dyn = (Elf64_Dyn *)((char *)elf + dyn_off);
        size_t cnt = dyn_sz / sizeof(Elf64_Dyn);
        for (size_t j = 0; j < cnt; j++) {
            if (dyn[j].d_tag == DT_BIND_NOW) {
                has_bind_now = 1;
                break;
            }
            if (dyn[j].d_tag == DT_FLAGS_1 &&
                (dyn[j].d_un.d_val & DF_1_NOW)) {
                has_bind_now = 1;
                break;
            }
            if (dyn[j].d_tag == DT_NULL) {
                break;
            }
        }
    }

    if (has_relro && has_bind_now)
        printf("RELRO:   %19s\n", "Full RELRO");
    else if (has_relro)
        printf("RELRO:   %19s\n", "Partial RELRO");
    else
        printf("RELRO:   %19s\n", "No RELRO");
}

void check_canary(void *elf, Elf64_Ehdr *ehdr, Elf64_Shdr *shdr) {
    Elf64_Shdr *shstr_hdr = &shdr[ehdr->e_shstrndx];
    const char *shstrtab = (const char *)elf + shstr_hdr->sh_offset;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *section_name = shstrtab + shdr[i].sh_name;
        if (strcmp(section_name, ".symtab") == 0 || strcmp(section_name, ".dynsym") == 0) {
            Elf64_Shdr *symtab = &shdr[i];
            Elf64_Shdr *strtab = &shdr[symtab->sh_link];

            Elf64_Sym *symbols = (Elf64_Sym *)((char *)elf + symtab->sh_offset);
            const char *strtab_base = (char *)elf + strtab->sh_offset;
            int symbol_count = symtab->sh_size / sizeof(Elf64_Sym);

            for (int j = 0; j < symbol_count; j++) {
                const char * name = strtab_base + symbols[j].st_name;
                if (strcmp(name, "__stack_chk_fail") == 0) {
                    printf("Stack:   %19s\n", "Canary found");
                    return;
                }
            }
        }
    }
    printf("Stack:   %19s\n", "No canary found");
}

void check_NX(Elf64_Ehdr *ehdr, Elf64_Phdr *phdr) {
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_GNU_STACK) {
            if (phdr[i].p_flags & PF_X) {
                printf("NX:     %20s\n", "No");
            } 
            else {
                printf("NX:     %20s\n", "Yes");
            }
            return;
        }
    }
    printf("NX:     %20s\n", "Unknown");
}

void check_PIE(Elf64_Ehdr *ehdr, Elf64_Phdr *phdr) {
    if (ehdr->e_type == ET_DYN) {
        int has_interp = 0;
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_INTERP) {
                has_interp = 1;
                break;
            }
        }
        if (has_interp) {
            printf("PIE:     %19s\n", "Yes");
        } else {
            printf("PIE:     %19s\n", "DSO (shared object)");
        }
    }
    else if (ehdr->e_type == ET_EXEC) {
        printf("PIE:     %19s\n", "No");
    }
    else {
        printf("PIE:     Unknown e_type: 0x%x\n", ehdr->e_type);
    }
}

void check_SHSTK_IBT(void *elf, Elf64_Ehdr *ehdr, Elf64_Phdr *phdr) {
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_NOTE)
            continue;

        uint8_t *segment = (uint8_t *)elf + phdr[i].p_offset;
        size_t size = phdr[i].p_filesz;

        while (size >= sizeof(Elf64_Nhdr)) {
            Elf64_Nhdr *nhdr = (Elf64_Nhdr *)segment;
            segment += sizeof(Elf64_Nhdr);
            size    -= sizeof(Elf64_Nhdr);

            size_t namesz_aligned = (nhdr->n_namesz + 3) & ~3;
            if (size < namesz_aligned) break;
            const char *name = (const char *)segment;
            segment += namesz_aligned;
            size    -= namesz_aligned;

            size_t descsz_aligned = (nhdr->n_descsz + 3) & ~3;
            if (size < descsz_aligned) break;
            const uint8_t *desc = segment;
            segment += descsz_aligned;
            size    -= descsz_aligned;

            if (nhdr->n_type == NT_GNU_PROPERTY_TYPE_0 &&
                nhdr->n_namesz == 4 && strncmp(name, "GNU", 3) == 0) {

                size_t offset = 0;
                while (offset + 8 <= nhdr->n_descsz) {
                    uint32_t type   = *(uint32_t *)(desc + offset);
                    uint32_t datasz = *(uint32_t *)(desc + offset + 4);
                    offset += 8;

                    if (offset + datasz > nhdr->n_descsz) break;

                    if (type == GNU_PROPERTY_X86_FEATURE_1_AND && datasz >= 4) {
                        uint32_t features = *(uint32_t *)(desc + offset);
                        printf("IBT:      %18s\n", (features & GNU_PROPERTY_X86_FEATURE_1_IBT) ? "Enabled" : "Disabled");
                        printf("SHSTK:    %18s\n", (features & GNU_PROPERTY_X86_FEATURE_1_SHSTK) ? "Enabled" : "Disabled");
                        return;
                    }

                    offset += (datasz + 3) & ~3;
                }
            }
        }
    }

    printf("IBT:      %18s\n", "Unknown");
    printf("SHSTK:    %18s\n", "Unknown");
}

void check_Stripped(void *elf, Elf64_Ehdr *ehdr, Elf64_Shdr *shdr) {
    Elf64_Shdr *shstr_hdr = &shdr[ehdr->e_shstrndx];
    const char *shstrtab = (const char *)elf + shstr_hdr->sh_offset;
    int found_symtab = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *name = shstrtab + shdr[i].sh_name;
        if (strcmp(name, ".symtab") == 0) {
            found_symtab = 1;
            break;
        }
    }

    if (found_symtab)
        printf("Stripped: %18s\n", "NO");
    else
        printf("Stripped: %18s\n", "Yes");
}

int main(int argc, char *argv[]) {
    int fd;
    struct stat st;
    void *elf;
    Elf64_Ehdr *ehdr;
    Elf64_Phdr *phdr;
    Elf64_Shdr *shdr;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path/to/elf>\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return 1;
    }

    elf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (elf == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    ehdr = (Elf64_Ehdr *)elf;
    phdr = (Elf64_Phdr *)((char *)elf + ehdr->e_phoff);
    shdr = (Elf64_Shdr *)((char *)elf + ehdr->e_shoff);

    check_RELRO(elf, ehdr, phdr, shdr);
    check_canary(elf, ehdr, shdr);
    check_NX(ehdr, phdr);
    check_PIE(ehdr, phdr);
    check_SHSTK_IBT(elf, ehdr, phdr);
    check_Stripped(elf, ehdr, shdr);

    munmap(elf, st.st_size);
    close(fd);
    return 0;
}