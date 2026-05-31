/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = pvma->vm_id;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
    vmait = pvma->vm_id;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn, addr_t swpfpn)
{
  __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct *newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  // struct vm_area_struct *cur_vma = get_vma_by_num(caller->kernl->mm, vmaid);

  // newrg = malloc(sizeof(struct vm_rg_struct));

  /* TODO: update the newrg boundary
  // newrg->rg_start = ...
  // newrg->rg_end = ...
  */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;
  /* END TODO */

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  // struct vm_area_struct *vma = caller->krnl->mm->mmap;

  /* TODO validate the planned memory area is not overlapped */
  if (vmastart >= vmaend)
  {
    return -1;
  }

  struct vm_area_struct *vma = caller->krnl->mm->mmap;
  if (vma == NULL)
  {
    return -1;
  }

  /* TODO validate the planned memory area is not overlapped */

  struct vm_area_struct *cur_area = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_area == NULL)
  {
    return -1;
  }

  while (vma != NULL)
  {
    if (vma != cur_area && OVERLAP(cur_area->vm_start, cur_area->vm_end, vma->vm_start, vma->vm_end))
    {
      return -1;
    }
    vma = vma->vm_next;
  }
  /* End TODO*/

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  /* Lấy VMA hiện tại dựa trên vmaid */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL) {
    return -1;
  }

  /* TODO 1: Align kích thước cần tăng và Tính vùng mới */
#ifdef MM64
  addr_t inc_amt = PAGING64_PAGE_ALIGNSZ(inc_sz);
  int incnumpage = inc_amt / PAGING64_PAGESZ;
#else
  addr_t inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage = inc_amt / PAGING_PAGESZ;
#endif

  addr_t old_end = cur_vma->vm_end;
  addr_t new_end = old_end + inc_amt;

  /* TODO 2: Kiểm tra overlap */
  /* Kiểm tra xem việc mở rộng [old_end, new_end) có đè lên vùng nhớ (VMA) nào khác không */
  if (validate_overlap_vm_area(caller, vmaid, old_end, new_end) < 0) {
    return -1; /* Phát hiện overlap, không thể mở rộng */
  }

  /* Tạo region mô tả vùng mở rộng */
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  newrg->rg_start = old_end;
  newrg->rg_end = new_end;
  newrg->rg_next = NULL;

  /* Map virtual → physical 
   * (Sử dụng vm_map_ram theo thư viện của hệ điều hành, thay vì vm_map_range) */
  if (vm_map_range(caller, newrg->rg_start, newrg->rg_end, old_end, incnumpage, newrg) < 0) {
    free(newrg);
    return -1; /* Lỗi cấp phát khung trang vật lý (có thể hết RAM) */
  }

  /* Cập nhật giới hạn VMA: trước: [0, 8192) -> sau:[0, 12288) */
  cur_vma->vm_end = new_end;

  /* Update free region list */
  /* Đưa vùng nhớ mới xin được (vẫn còn trống) vào cuối danh sách freerg_list của VMA */
  if (cur_vma->vm_freerg_list == NULL) {
    cur_vma->vm_freerg_list = newrg;
  } else {
    struct vm_rg_struct *rg = cur_vma->vm_freerg_list;
    struct vm_rg_struct *prev_rg = NULL;
    while (rg->rg_next != NULL) { prev_rg = rg; rg = rg->rg_next; }

    if (rg->rg_start >= rg->rg_end) {
        /* Remove the stale zero-size tail */
        if (prev_rg == NULL)
            cur_vma->vm_freerg_list = NULL;
        else
            prev_rg->rg_next = NULL;
        free(rg);
        /* Now append to the cleaned-up list */
        if (cur_vma->vm_freerg_list == NULL) {
            cur_vma->vm_freerg_list = newrg;
        } else {
            struct vm_rg_struct *tail = cur_vma->vm_freerg_list;
            while (tail->rg_next != NULL) tail = tail->rg_next;
            tail->rg_next = newrg;
        }
    } else {
        rg->rg_next = newrg;
    }
  }
  return 0;
}
int is_frame_free(struct memphy_struct *mp, addr_t fpn) {
    struct framephy_struct *curr = mp->free_fp_list;
    while (curr != NULL) {
        if (curr->fpn == fpn) return 1;
        curr = curr->fp_next;
    }
    return 0;
}

/* Helper: Rút một FPN cụ thể ra khỏi danh sách trống */
int extract_free_frame(struct memphy_struct *mp, addr_t fpn) {
    struct framephy_struct *curr = mp->free_fp_list;
    struct framephy_struct *prev = NULL;
    while (curr != NULL) {
        if (curr->fpn == fpn) {
            if (prev == NULL) {
                mp->free_fp_list = curr->fp_next;
            } else {
                prev->fp_next = curr->fp_next;
            }
            free(curr); /* Xoá node cũ */
            return 0;
        }
        prev = curr;
        curr = curr->fp_next;
    }
    return -1;
}

/* HÀM CHÍNH: Cấp phát N khung RAM liên tiếp */
addr_t alloc_contiguous_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
    struct memphy_struct *mram = caller->krnl->mram;
    
    /* Tính số lượng khung RAM tối đa của hệ thống để làm giới hạn quét */
#ifdef MM64
    int max_fpn = mram->maxsz / PAGING64_PAGESZ;
#else
    int max_fpn = mram->maxsz / PAGING_PAGESZ;
#endif

    int start_fpn = -1;

    /* 1. Thuật toán quét tìm vùng liên tục (First-Fit Contiguous) */
    for (int i = 0; i <= max_fpn - req_pgnum; i++) {
        int is_contiguous_free = 1;
        
        /* Kiểm tra xem N frames tính từ i có trống hết không */
        for (int j = 0; j < req_pgnum; j++) {
            if (!is_frame_free(mram, i + j)) {
                is_contiguous_free = 0;
                break;
            }
        }
        
        /* Nếu tìm thấy đủ N frames liên tiếp, chốt vị trí bắt đầu */
        if (is_contiguous_free) {
            start_fpn = i;
            break;
        }
    }

    if (start_fpn == -1) {
        return -1; /* Lỗi: Không tìm được dải RAM vật lý nào liên tục đủ lớn (Bị phân mảnh ngoại) */
    }

    /* 2. Trích xuất các frames đó và đưa vào danh sách trả về */
    struct framephy_struct *head = NULL;
    struct framephy_struct *tail = NULL;

    for (int j = 0; j < req_pgnum; j++) {
        addr_t target_fpn = start_fpn + j;
        
        /* Rút frame này ra khỏi kho trống */
        extract_free_frame(mram, target_fpn);

        /* Tạo node để gắn vào danh sách trả về */
        struct framephy_struct *newfp = malloc(sizeof(struct framephy_struct));
        newfp->fpn = target_fpn;
        newfp->fp_next = NULL;

        if (head == NULL) {
            head = newfp;
            tail = newfp;
        } else {
            tail->fp_next = newfp;
            tail = newfp;
        }

        /* Đồng thời cập nhật used_fp_list (Đánh dấu đã sử dụng) */
        struct framephy_struct *used_node = malloc(sizeof(struct framephy_struct));
        used_node->fpn = target_fpn;
        used_node->fp_next = mram->used_fp_list;
        mram->used_fp_list = used_node;
    }

    *frm_lst = head;
    return 0;
}

// #endif
