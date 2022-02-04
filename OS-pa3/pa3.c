/**********************************************************************
 * Copyright (c) 2020-2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "types.h"
#include "list_head.h"
#include "vm.h"

/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;

/**
 * TLB of the system.
 */
extern struct tlb_entry tlb[1UL << (PTES_PER_PAGE_SHIFT * 2)];

/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];

/**
 * lookup_tlb(@vpn, @pfn)
 *
 * DESCRIPTION
 *   Translate @vpn of the current process through TLB. DO NOT make your own
 *   data structure for TLB, but use the defined @tlb data structure
 *   to translate. If the requested VPN exists in the TLB, return true
 *   with @pfn is set to its PFN. Otherwise, return false.
 *   The framework calls this function when needed, so do not call
 *   this function manually.
 *
 * RETURN
 *   Return true if the translation is cached in the TLB.
 *   Return false otherwise
 */
bool lookup_tlb(unsigned int vpn, unsigned int *pfn)
{
	return false;
}

/**
 * insert_tlb(@vpn, @pfn)
 *
 * DESCRIPTION
 *   Insert the mapping from @vpn to @pfn into the TLB. The framework will call
 *   this function when required, so no need to call this function manually.
 *
 */
void insert_tlb(unsigned int vpn, unsigned int pfn)
{
}

/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with RW_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with RW_READ only should not be accessed with
 *   RW_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */
// 가장 적은 pfn에 할당하는 것이 중요하다

unsigned int alloc_page(unsigned int vpn, unsigned int rw)
{

	int outer_index = 0;
	int vpn_index = vpn % NR_PTES_PER_PAGE;

	if (!vpn)
		outer_index = vpn / NR_PTES_PER_PAGE;

	if (ptbr->outer_ptes[outer_index] == NULL)
	{
		(*ptbr).outer_ptes[outer_index] = (struct pte_directory *)malloc(sizeof(struct pte_directory));

		for (int i = 0; i < NR_PTES_PER_PAGE; i++)
		{
			//PTES 하나하나에 vaild, writable,private,pfn을 default로 할당해주면 된다.
			(*ptbr).outer_ptes[outer_index]->ptes[i].valid = false;
			(*ptbr).outer_ptes[outer_index]->ptes[i].writable = false;
			(*ptbr).outer_ptes[outer_index]->ptes[i].private = 0;
			(*ptbr).outer_ptes[outer_index]->ptes[i].pfn = 0;
		}
	}

	//valid 값이 유효하지 않다면?
	if ((*(*ptbr).outer_ptes[outer_index]).ptes[vpn_index].valid == false)
	{
		int min_pfn = 0;
		while (mapcounts[min_pfn] && min_pfn < NR_PAGEFRAMES)
		{
			min_pfn++;
		}
		//allocate 가능한 최소의 pfn Number을 뽑아내고 만약 모두 꽉차있ㄴ다면 NR_PAGEFAMES를 return 한다.

		if (min_pfn != NR_PAGEFRAMES)
		{
			mapcounts[min_pfn]++;
			//pageFrames와 같지 않을 경우에 대한 처리를 진행한다.
			(*ptbr).outer_ptes[outer_index]->ptes[vpn_index].valid = true;
			if (rw & RW_WRITE)
			{
				(*ptbr).outer_ptes[outer_index]->ptes[vpn_index].writable = true;
			}
			else
			{
				(*ptbr).outer_ptes[outer_index]->ptes[vpn_index].writable = false;
			}
			(*ptbr).outer_ptes[outer_index]->ptes[vpn_index].pfn = min_pfn;
			(*ptbr).outer_ptes[outer_index]->ptes[vpn_index].private = 1;

			current->pagetable = *ptbr;
			return min_pfn;
		}
	}
	return -1;
}
/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, writable, pfn) is set @false or 0.
 *   Also, consider carefully for the case when a page is shared by two processes,
 *   and one process is to free the page.
 */
void free_page(unsigned int vpn)
{
	int outer_index = 0;
	int vpn_index = vpn % NR_PTES_PER_PAGE;

	if (!vpn)
		outer_index = vpn / NR_PTES_PER_PAGE;

	mapcounts[ptbr->outer_ptes[outer_index]->ptes[vpn_index].pfn]--;

	ptbr->outer_ptes[outer_index]->ptes[vpn_index].valid = false;
	ptbr->outer_ptes[outer_index]->ptes[vpn_index].writable = false;
	ptbr->outer_ptes[outer_index]->ptes[vpn_index].private = 0;
	ptbr->outer_ptes[outer_index]->ptes[vpn_index].pfn = 0;

	int i = 0;
	while (i < NR_PTES_PER_PAGE)
	{
		if (ptbr->outer_ptes[outer_index]->ptes[i].valid)
		{
			return;
		}
		else
		{
			if (ptbr->outer_ptes[outer_index]->ptes[i].pfn != 0)
			{
				return;
			}
		}
	}
	free(ptbr->outer_ptes[outer_index]);
	ptbr->outer_ptes[outer_index] = NULL;
}

/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid
 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
	return false;
}

/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process.
 *   The @current process at the moment should be put into the @processes
 *   list, and @current should be replaced to the requested process.
 *   Make sure that the next process is unlinked from the @processes, and
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table. 
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You may use pte->private for 
 *   storing some useful information :-)
 */
void switch_process(unsigned int pid)
{
}
