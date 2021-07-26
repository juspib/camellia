#pragma once

#include <kernel/arch/i386/registers.h>
#include <kernel/arch/log.h>
#include <stdbool.h>

// i have no idea where else to put it
// some code assumes that it's a power of 2
#define PAGE_SIZE 4096
#define PAGE_MASK (PAGE_SIZE - 1)

// linker.ld
extern char _bss_end;

__attribute__((noreturn))
void halt_cpu();

// src/arch/i386/sysenter.s
_Noreturn void sysexit(struct registers);
int syscall_handler(int, int, int, int);

// all of those can allocate memory
struct pagedir *pagedir_new();
struct pagedir *pagedir_copy(const struct pagedir *orig);
void pagedir_map(struct pagedir *dir, void *virt, void *phys,
                 bool user, bool writeable);

void pagedir_switch(struct pagedir *);

// return 0 on failure
void *pagedir_virt2phys(struct pagedir *dir, const void *virt,
                        bool user, bool writeable);
