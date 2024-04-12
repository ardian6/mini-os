/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->level_1_page_table = kmalloc(2048 * sizeof(paddr_t *));

	if (as->level_1_page_table == NULL) {
		kfree(as);
		return NULL;
	}
	int i = 0;
	while (i < 2048) {
		as->level_1_page_table[i] = NULL;
		i = i + 1;
	}
	as->region_head = NULL;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */
	// Brute force copy for 2-level page table
	for (int i = 0; i < 2048; i++) {
		for (int j = 0; j < 512; j++) {
			if (old->level_1_page_table[i] == NULL) {
				break;
			}
			else if (old->level_1_page_table[i][j] != 0) { // Could be NULL instead of 0 here
				memmove((void *)newas->level_1_page_table[i][j], (void *)old->level_1_page_table[i][j], PAGE_SIZE);
			}
		}
	}
	// Brute force copy for all the region structs
	struct region_struct *temp;
	temp = old->region_head;
	struct region_struct *curr_region;
	curr_region = NULL;
	while (temp != NULL) {
		struct region_struct *newRegion = kmalloc(sizeof(struct region_struct *));
		if (newRegion == NULL) {
			return ENOMEM;
		}
		newRegion->vir_start = temp->vir_start;
        newRegion->region_size = temp->region_size;
        newRegion->readable = temp->readable;
        newRegion->writeable = temp->writeable;
        newRegion->executable = temp->executable; 
		newRegion->correct_status = temp->correct_status;
        newRegion->next_region = NULL;
		if (newas->region_head == NULL){
			newas->region_head = newRegion;
		} else {
			curr_region->next_region = newRegion;
		}
		curr_region = newRegion;
		temp = temp->next_region;
	}
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	// Brute force free of all level 2 tables first then the level 1 table
	for (int i = 0; i < 2048; i++) {
		if (as->level_1_page_table[i] != NULL) {
			for (int j = 0; j < 512; j++) {
				if (as->level_1_page_table[i][j] != 0) {
					free_kpages(PADDR_TO_KVADDR(as->level_1_page_table[i][j]));
				}
			}
			kfree(as->level_1_page_table[i]);
		}	
	}
	kfree(as->level_1_page_table);
	// Brute force free of all the structs for regions
	struct region_struct *temp;
	temp = as->region_head;
	while (temp != NULL) {
		struct region_struct *dup;
		dup = temp;
		temp = temp->next_region;
		kfree(dup);
	}
	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
	// Copied from dumbvm
	/* Disable interrupts on this CPU while frobbing the TLB. */
	int spl = splhigh();

	for (int i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */

	// Copied from as_activate
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
	// Copied from dumbvm
	/* Disable interrupts on this CPU while frobbing the TLB. */
	int spl = splhigh();

	for (int i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	// 
	// CHECK FOR REGION OVERLAP ERROR
	//

	// Copied from dumbvm
	/* Align the region. First, the base... */
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;
	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	struct region_struct *newRegion = kmalloc(sizeof(struct region_struct *));
	if (newRegion == NULL) {
		return ENOMEM;
	}
	newRegion->vir_start = vaddr;
	newRegion->region_size = memsize;
	newRegion->readable = readable;
	newRegion->writeable = writeable;
	newRegion->executable = executable;
	newRegion->correct_status = writeable;
	newRegion->next_region = NULL;

	if (as->region_head == NULL) {
		as->region_head = newRegion;
	} else {
		struct region_struct *temp;
		temp = as->region_head;
		while (temp->next_region != NULL) {
			temp = temp->next_region;
		}
		temp->next_region = newRegion;
	}

	return 0; /* Unimplemented */
}

int
as_prepare_load(struct addrspace *as)
{

	struct region_struct *temp;
	temp = as->region_head;
	while (temp != NULL) {
		temp->writeable = 1;
		temp = temp->next_region;
	}
	return 0;
}

int
as_complete_load(struct addrspace *as)
{

	struct region_struct *temp;
	temp = as->region_head;
	while (temp!= NULL) {
		temp->writeable = temp->correct_status;
		temp = temp->next_region;
	}
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	int err = as_define_region(as, USERSTACK - (16 * PAGE_SIZE), 16 * PAGE_SIZE, 1, 1, 1);
	if (err != 0) {
		return err;
	}

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

