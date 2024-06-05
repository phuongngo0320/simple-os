
#include "cpu.h"
#include "mem.h"
#include "mm.h"
#ifdef IODUMP
#include <stdio.h>
#include <stdlib.h>
#define INST_MAX_SIZE 100
#endif

int calc(struct pcb_t * proc) {
	return ((unsigned long)proc & 0UL);
}

int alloc(struct pcb_t * proc, uint32_t size, uint32_t reg_index) {
	addr_t addr = alloc_mem(size, proc);
	if (addr == 0) {
		return 1;
	}else{
		proc->regs[reg_index] = addr;
		return 0;
	}
}

int free_data(struct pcb_t * proc, uint32_t reg_index) {
	return free_mem(proc->regs[reg_index], proc);
}

int read(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) { // Index of destination register
	
	BYTE data;
	if (read_mem(proc->regs[source] + offset, proc,	&data)) {
		proc->regs[destination] = data;
		return 0;		
	}else{
		return 1;
	}
}

int write(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset) { 	// Destination address =
					// [destination] + [offset]
	return write_mem(proc->regs[destination] + offset, proc, data);
} 

int run(struct pcb_t * proc) {
	#ifdef IODUMP
	char* inst = malloc(INST_MAX_SIZE * sizeof(char));
	#endif

	/* Check if Program Counter point to the proper instruction */
	if (proc->pc >= proc->code->size) {
		#ifdef IODUMP
		free(inst);
		#endif
		return 1;
	}
	
	struct inst_t ins = proc->code->text[proc->pc];
	#ifdef IODUMP
	uint32_t a0 = ins.arg_0;
	uint32_t a1 = ins.arg_1;
	uint32_t a2 = ins.arg_2;
	#endif
	proc->pc++;
	int stat = 1;
	switch (ins.opcode) {
	case CALC:
		#ifdef MEM_DEBUG
		printf("---CALC---\n");
		#endif
		stat = calc(proc);
		break;
	case ALLOC:
#ifdef MM_PAGING
		#ifdef IODUMP
		sprintf(inst, "---ALLOC %u %u---", a0, a1);
		#ifdef MEM_DEBUG
		printf("%s\n", inst);
		#endif
		#endif
		stat = pgalloc(proc, ins.arg_0, ins.arg_1);

// -------------------------------------------------------------------
// print page table
#ifdef IODUMP
print_pgtbl(proc, 0, -1);
// show status of RAM
MEMPHY_dump(proc->mram, "ram.txt", inst);
// show status of MEMSWAPs
MEMPHY_dump(&proc->mswp[0], "swp0.txt", inst);
MEMPHY_dump(&proc->mswp[1], "swp1.txt", inst);
MEMPHY_dump(&proc->mswp[2], "swp2.txt", inst);
MEMPHY_dump(&proc->mswp[3], "swp3.txt", inst);
#endif

// -------------------------------------------------------------------
#else
		stat = alloc(proc, ins.arg_0, ins.arg_1);
#endif
		break;
	case FREE:
#ifdef MM_PAGING
		#ifdef IODUMP
		sprintf(inst, "---FREE %u---", a0);
		#ifdef MEM_DEBUG
		printf("%s\n", inst);
		#endif
		#endif
		stat = pgfree_data(proc, ins.arg_0);
// -------------------------------------------------------------------
// print page table
#ifdef IODUMP
print_pgtbl(proc, 0, -1);
// show status of RAM
MEMPHY_dump(proc->mram, "ram.txt", inst);
// show status of MEMSWAPs
MEMPHY_dump(&proc->mswp[0], "swp0.txt", inst);
MEMPHY_dump(&proc->mswp[1], "swp1.txt", inst);
MEMPHY_dump(&proc->mswp[2], "swp2.txt", inst);
MEMPHY_dump(&proc->mswp[3], "swp3.txt", inst);
#endif

// -------------------------------------------------------------------
#else
		stat = free_data(proc, ins.arg_0);
#endif
		break;
	case READ:
#ifdef MM_PAGING
		#ifdef IODUMP
		sprintf(inst, "---READ %u %u %u---", a0, a1, a2);
		#ifdef MEM_DEBUG
		printf("%s\n", inst);
		#endif
		#endif
		stat = pgread(proc, ins.arg_0, ins.arg_1, ins.arg_2);
// -------------------------------------------------------------------f
#ifdef IODUMP
// show status of RAM
MEMPHY_dump(proc->mram, "ram.txt", inst);
// show status of MEMSWAPs
MEMPHY_dump(&proc->mswp[0], "swp0.txt", inst);
MEMPHY_dump(&proc->mswp[1], "swp1.txt", inst);
MEMPHY_dump(&proc->mswp[2], "swp2.txt", inst);
MEMPHY_dump(&proc->mswp[3], "swp3.txt", inst);
#endif

// -------------------------------------------------------------------
#else
		stat = read(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
		break;
	case WRITE:
#ifdef MM_PAGING
		#ifdef IODUMP
		sprintf(inst, "---WRITE %u %u %u---", a0, a1, a2);
		#ifdef MEM_DEBUG
		printf("%s\n", inst);
		#endif
		#endif
		stat = pgwrite(proc, ins.arg_0, ins.arg_1, ins.arg_2);
// -------------------------------------------------------------------
#ifdef IODUMP
// show status of RAM
MEMPHY_dump(proc->mram, "ram.txt", inst);
// show status of MEMSWAPs
MEMPHY_dump(&proc->mswp[0], "swp0.txt", inst);
MEMPHY_dump(&proc->mswp[1], "swp1.txt", inst);
MEMPHY_dump(&proc->mswp[2], "swp2.txt", inst);
MEMPHY_dump(&proc->mswp[3], "swp3.txt", inst);
#endif

// -------------------------------------------------------------------
#else
		stat = write(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
		break;
	default:
		stat = 1;
	}
	#ifdef IODUMP
	free(inst);
	#endif
	return stat;

}


