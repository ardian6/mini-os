#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <proc.h>
#include <spl.h>

/* Place your page table functions here */


void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    /*
    (void) faulttype;
    (void) faultaddress;

    panic("vm_fault hasn't been written yet\n");
    */
    
    // Check for readonly and return EFAULT if yes
    if (faulttype == VM_FAULT_READONLY) {
        return EFAULT;
    }

    // MIGHT NOT WORK
    // most_sig_11_bits = top level PT number 
    // most_sig_9_bits = 2nd level PT number
    vaddr_t most_sig_11_bits = faultaddress >> 21;
    vaddr_t most_sig_9_bits = (faultaddress << 11) >> 23;
    vaddr_t offset = (faultaddress << 20) >> 20;

    struct addrspace *as = proc_getas();
    if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

    int spl;
    if (as->level_1_page_table[most_sig_11_bits] != NULL) {
        if (as->level_1_page_table[most_sig_11_bits][most_sig_9_bits] != 0) {
            // Might have to convert from paddr to vaddr
            paddr_t frame_num = KVADDR_TO_PADDR(as->level_1_page_table[most_sig_11_bits][most_sig_9_bits]);
            paddr_t entrylo = offset + (frame_num << 12);
            spl = splhigh();
            tlb_random(faultaddress, entrylo);
            splx(spl);
            return 0;
        }
    }

    if (as->region_head == NULL) {
        return EFAULT;
    }

    // Check for invalid regions
    vaddr_t top;
    size_t end;
    
    struct region_struct *cur;
    cur = as->region_head;

    int found = 0;
    while (cur != NULL) {
        top = cur->vir_start;
        end = top + cur->region_size;
        if (faultaddress >= top && faultaddress < end) {
            found = 1;
        }

        cur = cur->next_region;   
    }

    if (found == 0) {
        return EFAULT;
    }

    // Translation Not Valid, Region Valid

    if (as->level_1_page_table[most_sig_11_bits] == NULL) {
        as->level_1_page_table[most_sig_11_bits] = kmalloc(512 * sizeof(paddr_t));
    }
    // Allocate frame
    as->level_1_page_table[most_sig_11_bits][most_sig_9_bits] = alloc_kpages(1);
    paddr_t frame_num = KVADDR_TO_PADDR(as->level_1_page_table[most_sig_11_bits][most_sig_9_bits]);
    paddr_t entrylo = offset + (frame_num << 12);

    spl = splhigh();
    tlb_random(faultaddress, entrylo);
    splx(spl);
    

    return 0;
}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

