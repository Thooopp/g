/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "syscall.h"
#include "libmem.h"
#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,       // present
             addr_t fpn,    // FPN
             int drt,       // dirty
             int swp,       // swap
             int swptyp,    // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0)
  {
    if (swp == 0)
    { // Non swap ~ page online
      if (fpn == 0)
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table
 */
int get_pd_from_address(addr_t addr, addr_t *pgd, addr_t *p4d, addr_t *pud, addr_t *pmd, addr_t *pt)
{
  /* Extract page direactories */
  *pgd = (addr & PAGING64_ADDR_PGD_MASK) >> PAGING64_ADDR_PGD_LOBIT;
  *p4d = (addr & PAGING64_ADDR_P4D_MASK) >> PAGING64_ADDR_P4D_LOBIT;
  *pud = (addr & PAGING64_ADDR_PUD_MASK) >> PAGING64_ADDR_PUD_LOBIT;
  *pmd = (addr & PAGING64_ADDR_PMD_MASK) >> PAGING64_ADDR_PMD_LOBIT;
  *pt = (addr & PAGING64_ADDR_PT_MASK) >> PAGING64_ADDR_PT_LOBIT;

  /* TODO: implement the page direactories mapping */

  return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table
 */
int get_pd_from_pagenum(addr_t pgn, addr_t *pgd, addr_t *p4d, addr_t *pud, addr_t *pmd, addr_t *pt)
{
  /* Shift the address to get page num and perform the mapping*/
  return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                             pgd, p4d, pud, pmd, pt);
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
  struct krnl_t *krnl = caller->krnl;

  addr_t *pte;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
	
  // dummy pte alloc to avoid runtime error
  pte = malloc(sizeof(addr_t));
#ifdef MM64	
  /* Get value from the system */
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  //... krnl->mm->pgd
  //... krnl->mm->pt
  pte = &krnl->mm->pt[pgn];
#else
  pte = &krnl->mm->pgd[pgn];
#endif
	
  CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
  struct krnl_t *krnl = caller->krnl;

  addr_t *pte;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
	
  // dummy pte alloc to avoid runtime error
  pte = malloc(sizeof(addr_t));
#ifdef MM64	
  /* Get value from the system */
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  //... krnl->mm->pgd
  //... krnl->mm->pt
  pte = &krnl->mm->pt[pgn];
#else
  pte = &krnl->mm->pgd[pgn];
#endif

  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
  struct krnl_t *krnl = caller->krnl;
  uint32_t pte = 0;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t	pt=0;
	
  /* TODO Perform multi-level page mapping */
#ifdef MM64
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  //... krnl->mm->pgd
  //... krnl->mm->pt
  pte = krnl->mm->pt[pgn];
#else
  pte = krnl->mm->pgd[pgn];	
#endif
  return pte;
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
	struct krnl_t *krnl = caller->krnl;
#ifdef MM64
  /* 64-bit: Cập nhật trực tiếp vào lớp Page Table (pt) cuối cùng */
  krnl->mm->pt[pgn] = pte_val;
#else
  /* 32-bit: Cập nhật vào Page Directory (pgd) */
  krnl->mm->pgd[pgn] = pte_val;
#endif
	
	return 0;
}

/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller, // process call
                    addr_t addr,          // start address which is aligned to pagesz
                    int pgnum)            // num of mapping page
{
  // int pgit = 0;
  // uint64_t pattern = 0xdeadbeef;

  /* TODO memset the page table with given pattern
   */

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                   // no guarantee all given pages are mapped
  struct framephy_struct *fpit = frames;
  addr_t pgn;

  /* TODO: Cập nhật vùng nhớ đã map */
  /* Ghi nhận địa chỉ bắt đầu của vùng nhớ ảo được cấp phát */
  ret_rg->rg_start = addr;
  
  /* Tính toán địa chỉ kết thúc của vùng nhớ ảo. 
   * Dùng PAGING_PAGESZ cho hệ 32-bit và PAGING64_PAGESZ cho hệ 64-bit */
#ifdef MM64
  ret_rg->rg_end = addr + pgnum * PAGING64_PAGESZ;
#else
  ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;
#endif

  /* TODO: Map từng page ảo với frame vật lý tương ứng */
  for (int pgit = 0; pgit < pgnum; pgit++)
  {
    /* Bảo vệ an toàn: Nếu danh sách frame bị đứt gãy giữa chừng (thiếu RAM vật lý) */
    if (fpit == NULL) {
      /* Cập nhật lại rg_end để phản ánh chính xác số lượng trang thực tế đã map */
#ifdef MM64
      ret_rg->rg_end = addr + pgit * PAGING64_PAGESZ;
#else
      ret_rg->rg_end = addr + pgit * PAGING_PAGESZ;
#endif
      break;
    }

    /* Tính số thứ tự page (Page Number) từ địa chỉ bắt đầu cộng thêm offset vòng lặp */
#ifdef MM64
    pgn = (addr / PAGING64_PAGESZ) + pgit; 
#else
    pgn = PAGING_PGN(addr) + pgit;
#endif

    /* Ghi ánh xạ page -> frame vào page table (đưa page này vào trạng thái "online" trong RAM) */
    pte_set_fpn(caller, pgn, fpit->fpn);

    /* Đưa vào FIFO (phục vụ cho thuật toán page replacement sau này nếu RAM bị đầy) */
    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);

    /* Tiến con trỏ tới frame vật lý tiếp theo để chuẩn bị map cho page tiếp theo */
    fpit = fpit->fp_next;
  }

  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  addr_t fpn;
  int i;
  addr_t vicpgn;
  uint32_t victim_pte;

  struct framephy_struct *head = NULL;
  struct framephy_struct *tail = NULL;

  for (i = 0; i < req_pgnum; i++)
  {
    /* Thử lấy một frame trống từ RAM */
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) != 0)
    {
      /* HẾT RAM! Cần thực hiện Page Replacement (Swapping) */
      
      /* 1. Chọn page bị đuổi (victim page) */
      if (find_victim_page(caller->krnl->mm, &vicpgn) < 0) {
        return -1; /* Out of memory (Không còn trang nào để swap) */
      }

      /* Lấy PTE của victim page để biết nó đang nằm ở frame vật lý nào */
      victim_pte = pte_get_entry(caller, vicpgn);
      addr_t victim_fpn = PAGING_FPN(victim_pte);

      struct framephy_struct *curr = caller->krnl->mram->used_fp_list;
      struct framephy_struct *prev = NULL;
      while (curr != NULL) {
          if (curr->fpn == victim_fpn) {
              if (prev == NULL) {
                  caller->krnl->mram->used_fp_list = curr->fp_next;
              } else {
                  prev->fp_next = curr->fp_next;
              }
              free(curr); /* Xóa node cũ */
              break;
          }
          prev = curr;
          curr = curr->fp_next;
      }

      /* 2. Lấy không gian trống trên đĩa SWAP */
      addr_t swp_off;
      if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swp_off) != 0) {
        return -1; /* Ổ cứng Swap cũng đầy -> Không thể cấp phát thêm */
      }
      
      /* Lấy ID của ổ Swap đang hoạt động */
      int swptyp = caller->krnl->active_mswp_id; 

      /* 3. Cập nhật used_fp_list của đĩa Swap */
      struct framephy_struct *swp_used_node = malloc(sizeof(struct framephy_struct));
      swp_used_node->fpn = swp_off;
      swp_used_node->owner = caller->krnl->mm;
      swp_used_node->fp_next = caller->krnl->active_mswp->used_fp_list;
      caller->krnl->active_mswp->used_fp_list = swp_used_node;

      /* 4. Thực hiện lệnh ghi dữ liệu từ RAM xuống đĩa Swap thông qua Syscall */
      struct sc_regs regs;
      regs.a1 = SYSMEM_SWP_OP; 
      regs.a2 = victim_fpn;
      regs.a3 = swp_off;
      _syscall(caller->krnl, caller->pid, 17, &regs); /* Syscall 17 là sys_memmap */

      /* 5. Cập nhật Page Table: Đánh dấu victim page đã bị đẩy ra đĩa */
      pte_set_swap(caller, vicpgn, swptyp, swp_off);

      /* 6. Thu hồi khung trang RAM vừa bị đẩy ra, sau đó lấy lại chính nó */
      MEMPHY_put_freefp(caller->krnl->mram, victim_fpn);
      MEMPHY_get_freefp(caller->krnl->mram, &fpn);
    }
// #ifdef MM64
//     int pagesz = PAGING64_PAGESZ;
// #else
//     int pagesz = PAGING_PAGESZ;
// #endif
//     for (int k = 0; k < pagesz; k++) {
//         MEMPHY_write(caller->krnl->mram, fpn * pagesz + k, 0);
//     }
    /* Đã có frame vật lý (fpn). Tạo một node mới để lưu vào danh sách trả về */
    struct framephy_struct *newfp = malloc(sizeof(struct framephy_struct));
    newfp->fpn = fpn;
    newfp->fp_next = NULL;

    /* Cập nhật head và tail của danh sách frame xin được */
    if (head == NULL) {
      head = newfp;
      tail = newfp;
    } else {
      tail->fp_next = newfp;
      tail = newfp;
    }

    /* Cập nhật used_fp_list của RAM: Đưa frame vừa lấy được vào danh sách đang sử dụng */
    struct framephy_struct *used_node = malloc(sizeof(struct framephy_struct));
    used_node->fpn = fpn;
    used_node->owner = caller->krnl->mm;
    used_node->fp_next = caller->krnl->mram->used_fp_list;
    caller->krnl->mram->used_fp_list = used_node;
  }

  /* Trả về danh sách frame đã lấy được */
  *frm_lst = head;
  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_range(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = 0;
  // int pgnum = incpgnum;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
  {
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

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

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_kernel(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  // struct krnl_t *krnl = caller->krnl; /* Xóa dòng này vì không còn gọi krnl trực tiếp */

  /* 1. Tính toán dung lượng bộ nhớ cần thiết để map */
  addr_t required_space;
#ifdef MM64
  required_space = incpgnum * PAGING64_PAGESZ;
#else
  required_space = incpgnum * PAGING_PAGESZ;
#endif

  /* 2. Kiểm tra Boundary */
  if (mapstart < astart || (mapstart + required_space) > aend) {
      return -1; 
  }

  /* 3. Xin RAM vật lý liên tục */
  if (alloc_contiguous_pages_range(caller, incpgnum, &frm_lst) != 0) {
      return -1; 
  }

  struct framephy_struct *fpit = frm_lst;
  addr_t current_kva = mapstart; 
  ret_rg->rg_start = mapstart;

  /* 4. Map từng page ảo vào frame vật lý */
  for (int i = 0; i < incpgnum; i++)
  {
    if (fpit == NULL) break; 

    addr_t fpn = fpit->fpn;
    addr_t pgn;

#ifdef MM64
    pgn = current_kva / PAGING64_PAGESZ;
#else
    pgn = PAGING_PGN(current_kva);
#endif

    /* FIX: Sử dụng hàm pte_set_fpn để HĐH tự động rẽ nhánh 32/64-bit
     * an toàn tuyệt đối và tránh Crash do truy cập mảng rỗng */
    pte_set_fpn(caller, pgn, fpn);

    fpit = fpit->fp_next;
#ifdef MM64
    current_kva += PAGING64_PAGESZ;
#else
    current_kva += PAGING_PAGESZ;
#endif
  }

  ret_rg->rg_end = current_kva;

  return mapstart;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
int cellidx;
  addr_t addrsrc, addrdst;
  
#ifdef MM64
  for (cellidx = 0; cellidx < PAGING64_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING64_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING64_PAGESZ + cellidx;
#else
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;
#endif
    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }
  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));

  /* TODO init page table directory */
  /* Cấp phát mảng Bảng trang (Page Table). 
   * Kích thước mảng dựa vào macro MAX_PGN định nghĩa trong mm.h / mm64.h
   */

#ifdef MM64
  /* Cấp phát bộ nhớ cho cả 5 cấp phân trang */
  mm->pgd = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
  mm->p4d = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
  mm->pud = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
  mm->pmd = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
  mm->pt  = malloc(PAGING64_MAX_PGN * sizeof(addr_t));

  /* Thiết lập các con trỏ móc xích từ cấp cao xuống cấp thấp.
   * Điều này sẽ thỏa mãn hoàn toàn logic dò tìm của hàm debug! */
  for (int i = 0; i < PAGING64_MAX_PGN; i++)
  {
      mm->pgd[i] = (addr_t)mm->p4d;
      mm->p4d[i] = (addr_t)mm->pud;
      mm->pud[i] = (addr_t)mm->pmd;
      mm->pmd[i] = (addr_t)mm->pt;
      mm->pt[i] = 0;
  }
#else
  mm->pgd = malloc(PAGING_MAX_PGN * sizeof(addr_t));
  mm->p4d = NULL;
  mm->pud = NULL;
  mm->pmd = NULL;
  mm->pt  = NULL;
#endif

  /* By default the owner comes with at least one vma */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->vm_freerg_list = NULL;
  vma0->sbrk = vma0->vm_start;

  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);
  /* TODO update VMA0 next */
  /* Do đây là VMA đầu tiên và duy nhất lúc khởi tạo, nó không có vùng kế tiếp */
  vma0->vm_next = NULL;

  /* Point vma owner backward */
  /* Liên kết ngược từ VMA về struct quản lý bộ nhớ của tiến trình */
  vma0->vm_mm = mm;

  /* TODO: update mmap */
  /* mm->mmap là con trỏ gốc quản lý danh sách liên kết các VMA */
  mm->mmap = vma0;
  
  /* Cập nhật symrgtbl (Bảng quản lý biến - Symbol Region Table)
   * Trong common.h, symrgtbl là một mảng tĩnh kích thước PAGING_MAX_SYMTBL_SZ
   * Ta chỉ cần khởi tạo các giá trị bên trong về 0 để tránh rác bộ nhớ.
   */
  for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++) {
      mm->symrgtbl[i].rg_start = 0;
      mm->symrgtbl[i].rg_end = 0;
  }

  /* Khởi tạo Kernel Cache Pool Table */
  mm->kcpooltbl = NULL;

  /* BẮT BUỘC: Khởi tạo hàng đợi FIFO cho Page Replacement. 
   * Nếu không gán NULL, khi Swapping kích hoạt sẽ bị crash do con trỏ rác. */
  mm->fifo_pgn = NULL;

  return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
struct pgn_t *pnode = malloc(sizeof(struct pgn_t));
    pnode->pgn = pgn;
    pnode->pg_next = NULL; // Luôn trỏ NULL ở cuối

    if (*plist == NULL) {
        *plist = pnode;
    } else {
        struct pgn_t *temp = *plist;
        while (temp->pg_next != NULL) temp = temp->pg_next;
        temp->pg_next = pnode;
    }
    return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  // addr_t pgn_start;//, pgn_end;
  // addr_t pgit;
  // struct krnl_t *krnl = caller->krnl;

  addr_t pgd = 0;
  addr_t p4d = 0;
  addr_t pud = 0;
  addr_t pmd = 0;
  addr_t pt = 0;

  get_pd_from_address(start, &pgd, &p4d, &pud, &pmd, &pt);

  /* TODO traverse the page map and dump the page directory entries */

  return 0;
}

#endif // def MM64
