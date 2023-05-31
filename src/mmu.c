#include "rvemu.h"

static void load_phdr(elf64_phdr_t *phdr, elf64_ehdr_t *ehdr, i64 i, FILE *file) {
    if (fseek(file, ehdr->e_phentsize * i,SEEK_SET) != 0) {
        fatal("seek file failed");
    }

    if (fread((void *)phdr, 1, sizeof(elf64_phdr_t), file) != sizeof(elf64_phdr_t)) {
        fatal("file too small");
    }
}

static int flags_to_mmap_prot(u32 flags) {
    return (flags & PF_R ? PROT_READ : 0) |
           (flags & PF_W ? PROT_WRITE : 0) |
           (flags & PF_R ? PROT_EXEC : 0);
}

static void mmu_load_segment(mmu_t *mmu, elf64_phdr_t *phdr, int fd) {
    int page_size = getpagesize();
    u64 offset = phdr->p_offset;
    u64 vaddr = TO_HOST(phdr->p_vaddr);
    u64 aligned_vaddr = ROUNDDOWN(vaddr, page_size);
    u64 filesz = phdr->p_filesz + (vaddr - aligned_vaddr);
    u64 memsz = phdr->p_memsz + (vaddr - aligned_vaddr);
    int prot = flags_to_mmap_prot(phdr->p_flags);
    u64 addr = (u64)mmap((void *)aligned_vaddr, filesz, prot, MAP_PRIVATE | MAP_FIXED,
                    fd, ROUNDDOWN(offset, page_size));
    assert(addr == aligned_vaddr);

    // .bss section
    u64 remaining_bss = ROUNDUP(memsz, page_size) - ROUNDUP(filesz, page_size);
    if (remaining_bss > 0) {
        u64 addr = (u64)mmap((void *)(aligned_vaddr + ROUNDUP(filesz, page_size)),
             remaining_bss, prot, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
        assert(addr == aligned_vaddr + ROUNDUP(filesz, page_size));
    }
    mmu->host_alloc = MAX(mmu->host_alloc, (aligned_vaddr + ROUNDUP(memsz, page_size)));

    mmu->base = mmu->alloc = TO_GUEST(mmu->host_alloc);
}

void mmu_load_elf(mmu_t *mmu, int fd) {
    u8 buf[sizeof(elf64_ehdr_t)];   // sizeof是读的结构体所有参数的字节数之和
    FILE *file = fdopen(fd, "rb");  // fd是一个文件描述符，“rb”表示以二进制模式打开文件，且只读访问
    // fread的第二个参数表示每次读一个字节，它的返回值是实际读取的数据块数
    if (fread(buf, 1, sizeof(elf64_ehdr_t), file) != sizeof(elf64_ehdr_t)) {
        fatal("file too small");
    }

    elf64_ehdr_t *ehdr = (elf64_ehdr_t *)buf;   // 将buf强制转换为elf64_ehdr_t的指针类型，并赋值给变量ehdr

    // *(u32 *)有什么作用，先强制转换再取指针？
    // 将ehdr转换为指向u32的指针，*进行解引用，可以读取4个字节的数据
    if (*(u32 *)ehdr != *(u32 *)ELFMAG) {
        fatal("bad elf file");
    }

    // 使用ident的第4位，因为ELF头文件的0\1\2\3位是固定的，为7f 45 4c 46
    if (ehdr->e_machine != EM_RISCV || ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        fatal("only riscv64 elf file is supported");
    }

    mmu->entry = (u64)ehdr->e_entry;

    elf64_phdr_t phdr;
    for (i64 i = 0; i < ehdr->e_phnum; i++) {
        load_phdr(&phdr, ehdr, i, file);

        if(phdr.p_type == PT_LOAD) {
            mmu_load_segment(mmu, &phdr, fd);
        }
    }

}