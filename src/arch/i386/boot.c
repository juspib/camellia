#include <arch/generic.h>
#include <arch/i386/gdt.h>
#include <arch/i386/interrupts/idt.h>
#include <arch/i386/multiboot.h>
#include <arch/i386/sysenter.h>
#include <arch/i386/tty.h>
#include <kernel/main.h>
#include <kernel/panic.h>

void kmain_early(struct multiboot_info *multiboot) {
	struct kmain_info info;

	// setup some basic stuff
	tty_clear();
	log_const("gdt...");
	gdt_init();
	log_const("idt...");
	idt_init();
	log_const("sysenter...");
	sysenter_setup();
	
	{ // find the init module
		struct multiboot_mod *module = &multiboot->mods[0];
		if (multiboot->mods_count < 1) {
			log_const("can't find init! ");
			panic();
		}
		info.init.at   = module->start;
		info.init.size = module->end - module->start;
	}
	
	kmain(info);
}
