#ifndef _CPU_CONTROL_AREA_H
#define _CPU_CONTROL_AREA_H

#include <types.h>
#include <atomic.h>
#include <cpu.h>

struct cpu_control_area {
	uval32 eye_catcher;
	uval32 thread_count;
	lock_t tlb_lock;
	uval32 tlb_index_randomizer;
	struct cpu *active_cpu;
};

extern void cpu_idle(void) __attribute__ ((noreturn));
extern uval cca_init(struct cpu_control_area *cca);
extern uval cca_table_init(uval n);
extern uval cca_table_enter(uval e, struct cpu_control_area *cca);
extern uval cca_hard_start(struct cpu *cpu, uval i, uval pc);
#endif /* _CPU_CONTROL_AREA_H */
