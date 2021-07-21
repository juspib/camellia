#pragma once

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
void sysexit(void (*fun)(), void *stack_top);
void sysenter_setup();
void syscall_handler();

// all of those can allocate memory
struct pagedir *pagedir_new();
void pagedir_map(struct pagedir *dir, void *virt, void *phys,
                 bool user, bool writeable);
void pagedir_switch(struct pagedir *);
