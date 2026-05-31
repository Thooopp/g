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
 * System Library
 * Memory Module Library libmem.c
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  if (rg_elmt == NULL)
    return -1;

  // Kiểm tra vùng nhớ hợp lệ
  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  struct vm_rg_struct *cur = mm->mmap->vm_freerg_list;
  struct vm_rg_struct *prev = NULL;

  /* Tìm vị trí chèn theo thứ tự tăng dần của địa chỉ bắt đầu */
  while (cur != NULL && cur->rg_start < rg_elmt->rg_start)
  {
    prev = cur;
    cur = cur->rg_next;
  }

  /* Chèn node mới vào giữa prev và cur */
  if (prev == NULL)
  {
    rg_elmt->rg_next = mm->mmap->vm_freerg_list;
    mm->mmap->vm_freerg_list = rg_elmt;
  }
  else
  {
    prev->rg_next = rg_elmt;
    rg_elmt->rg_next = cur;
  }

  /* TODO Gộp với vùng kế tiếp nếu liền kề */
  /* Kiểm tra xem điểm kết thúc của vùng vừa chèn có chạm ngay 
   * vào điểm bắt đầu của vùng kế tiếp hay không. */
  if (rg_elmt->rg_next != NULL && rg_elmt->rg_end == rg_elmt->rg_next->rg_start)
  {
    struct vm_rg_struct *next_node = rg_elmt->rg_next;
    
    /* Nới rộng rg_elmt bao trọn luôn next_node */
    rg_elmt->rg_end = next_node->rg_end;
    /* Cập nhật con trỏ để bỏ qua next_node */
    rg_elmt->rg_next = next_node->rg_next;
    
    /* Cực kỳ quan trọng: Giải phóng vùng nhớ struct đã bị gộp 
     * để tránh rò rỉ bộ nhớ (Memory Leak) */
    free(next_node); 
  }

  /* TODO Gộp với vùng phía trước nếu liền kề */
  /* Kiểm tra xem điểm kết thúc của vùng phía trước có chạm ngay 
   * vào điểm bắt đầu của vùng vừa chèn hay không. */
  if (prev != NULL && prev->rg_end == rg_elmt->rg_start)
  {
    /* Nới rộng prev bao trọn luôn rg_elmt 
     * (lúc này rg_elmt có thể đã gộp cả next_node ở bước trên) */
    prev->rg_end = rg_elmt->rg_end;
    /* Cập nhật con trỏ để bỏ qua rg_elmt */
    prev->rg_next = rg_elmt->rg_next;
    
    /* Giải phóng rg_elmt vì nó đã hòa làm một vào prev */
    free(rg_elmt); 
  }

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /* Khóa Mutex để đảm bảo an toàn khi hệ thống chạy đa luồng */
  pthread_mutex_lock(&mmvm_lock);
  
  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  int inc_sz = 0;

  /* 1. Cố gắng tìm một vùng nhớ trống trong danh sách (vm_freerg_list) hiện tại */
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    /* Cập nhật bảng quản lý biến (Symbol Region Table) */
    caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;
    
    /* TODO: UPDATE sbrk */
    /* Cờ sbrk đánh dấu đỉnh của heap. 
     * Chỉ đẩy đỉnh sbrk lên nếu vùng cấp phát này vượt qua sbrk hiện tại. */
    if (rgnode.rg_end > cur_vma->sbrk) {
        cur_vma->sbrk = rgnode.rg_end;
    }

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* 2. Nếu VMA đã hết vùng trống, chuẩn bị gói gọi Syscall để nhờ OS nới rộng VMA */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
  regs.a3 = size;
  
  /* TODO: SYSCALL 1 sys_memmap */
  /* Gọi System Call để xin OS cấp thêm bộ nhớ. 
   * (Lưu ý: ID của syscall có thể là 1 hoặc 17 tùy thuộc vào bảng syscall_table được định nghĩa trong bộ code của bạn) */
  int ret = _syscall(caller->krnl, caller->pid, 17, &regs); 
  if (ret < 0) {
      /* Nếu HĐH từ chối nới rộng (ví dụ hết RAM), trả về lỗi */
      pthread_mutex_unlock(&mmvm_lock);
      return -1; 
  }

  /* TODO: get_free_vmrg_area GIỐNG Ở TRÊN */
  /* Lúc này HĐH đã nới rộng VMA thành công và cập nhật lại vm_freerg_list.
   * Ta gọi lại hàm lấy vùng trống một lần nữa để lấy không gian vừa được nới rộng. */
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;

    /* Tiếp tục cập nhật sbrk nếu cần */
    if (rgnode.rg_end > cur_vma->sbrk) {
        cur_vma->sbrk = rgnode.rg_end;
    }
  } else {
      /* Trường hợp đã nới rộng thành công nhưng vẫn không lấy được vùng nhớ do lỗi logic */
      pthread_mutex_unlock(&mmvm_lock);
      return -1;
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* TODO: Manage the collect freed region to freerg_list */
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->krnl->mm, rgid);

  if (rgnode->rg_start == 0 && rgnode->rg_end == 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->krnl->mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t addr;
  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1)
  {
    return -1;
  }

#ifdef IODUMP
  /* TODO dump IO content (if needed) */
  printf("%s:%d pid=%u pc=%u alloc reg=%u size=%lu addr=0x%lx\n",
         __func__,
         __LINE__,
         proc->pid,
         proc->pc,
         reg_index,
         size,
         (unsigned long)addr);

#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  /* By default using vmaid = 0 */
  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);
  if (val == -1)
  {
    return -1;
  }

#ifdef IODUMP
  /* TODO dump IO content (if needed) */
  printf("%s:%d pid=%u pc=%u free reg=%u\n",
         __func__,
         __LINE__,
         proc->pid,
         proc->pc,
         reg_index);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif
  return 0; // val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = pte_get_entry(caller, pgn);

  /* Nếu CPU truy cập page không còn trong RAM (PRESENT == 0) -> Xảy ra PAGE FAULT */
  if (!PAGING_PAGE_PRESENT(pte))
  {
    addr_t tgt_fpn;
    
    /* ==============================================================
     * BƯỚC 1: Lấy frame trống HOẶC swap 1 page khác ra ngoài
     * ============================================================== */
    
    /* RAM còn trống -> Cấp frame trực tiếp */
    if (MEMPHY_get_freefp(caller->krnl->mram, &tgt_fpn) != 0)
    {
      /* RAM không còn trống -> Swap 1 victim page ra ngoài */
      addr_t vicpgn;
      if (find_victim_page(caller->krnl->mm, &vicpgn) == -1) {
        return -1; /* Lỗi nghiêm trọng: Không thể tìm được victim */
      }
      
      /* Lấy Frame vật lý mà nạn nhân đang chiếm giữ để tái sử dụng */
      uint32_t vicpte = pte_get_entry(caller, vicpgn);
      tgt_fpn = PAGING_FPN(vicpte); 

      /* Xin một Frame trống trên đĩa SWAP để chứa nạn nhân */
      addr_t vic_swpoff;
      if (MEMPHY_get_freefp(caller->krnl->active_mswp, &vic_swpoff) != 0) {
          return -1; /* Ổ đĩa SWAP cũng đã đầy */
      }

      /* Copy victim page từ RAM xuống đĩa SWAP */
      struct sc_regs regs;
      regs.a1 = SYSMEM_SWP_OP; 
      regs.a2 = tgt_fpn;        
      regs.a3 = vic_swpoff;
      _syscall(caller->krnl, caller->pid, 17, &regs);

      /* Cập nhật Page Table của victim: Đánh dấu đã bị đuổi xuống SWAP */
      pte_set_swap(caller, vicpgn, caller->krnl->active_mswp_id, vic_swpoff);
    }

    /* ==============================================================
     * BƯỚC 2: Copy page từ SWAP vào RAM (Swap-In)
     * ============================================================== */
    
    /* Trích xuất vị trí của page hiện tại đang nằm dưới SWAP từ PTE của nó */
    addr_t target_swpoff = GETVAL(pte, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    
    /* Copy dữ liệu từ đĩa SWAP vào cái Frame vật lý (tgt_fpn) vừa có được trên RAM */
    __swap_cp_page(caller->krnl->active_mswp, target_swpoff, caller->krnl->mram, tgt_fpn);

    /* Giải phóng vị trí trên đĩa SWAP (vì page này đã được kéo lên RAM an toàn) */
    MEMPHY_put_freefp(caller->krnl->active_mswp, target_swpoff);

    /* ==============================================================
     * BƯỚC 3: Cập nhật Page Table
     * ============================================================== */
    
    /* Cập nhật Page Table của page hiện tại: Đánh dấu đã có mặt trên RAM */
    pte_set_fpn(caller, pgn, tgt_fpn);

    /* Đưa page này vào FIFO để tham gia vào vòng đời Victim sau này */
    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
  }

  /* Cuối cùng, trả về Frame vật lý (fpn) để CPU tiến hành Read/Write */
  *fpn = PAGING_FPN(pte_get_entry(caller, pgn));

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn, off;

  /* TODO: Tính pgn và off trong PAGING64 */
#ifdef MM64
  pgn = (addr) >> 12;
  off = (addr) & ((1ULL << 12) - 1);
#else
  pgn = PAGING_PGN(addr);
  off = PAGING_OFFST(addr);
#endif

  int fpn;
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO: Tính địa chỉ vật lý (Physical Address) */
#ifdef MM64
  int phyaddr = (fpn * PAGING64_PAGESZ) + off;
#else
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off; 
#endif

  /* TODO: Gọi SYSCALL 17 sys_memmap để ra lệnh ĐỌC từ thiết bị nhớ */
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;  /* Mã thao tác đọc */
  regs.a2 = phyaddr;         /* Địa chỉ vật lý nguồn */
  
  _syscall(caller->krnl, caller->pid, 17, &regs);

  /* Kết quả đọc được lưu trong thanh ghi a3, trả về qua biến data */
  *data = (BYTE)regs.a3;
  
  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn, off;

  /* TODO: Tính pgn và off trong PAGING64 */
#ifdef MM64
  /* Với 64-bit, tính toán thông qua Kích thước trang PAGING64_PAGESZ */
  pgn = (addr) >> 12;
  off = (addr) & ((1ULL << 12) - 1);
#else
  /* Với 32-bit, có thể dùng macro mặc định của hệ thống */
  pgn = PAGING_PGN(addr);
  off = PAGING_OFFST(addr);
#endif

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  int fpn;
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO: Tính địa chỉ vật lý (Physical Address) */
#ifdef MM64
  int phyaddr = (fpn * PAGING64_PAGESZ) + off;
#else
  /* Dịch trái FPN hoặc nhân với PAGING_PAGESZ rồi cộng offset */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off; 
#endif

  /* TODO: Gọi SYSCALL 17 sys_memmap để ra lệnh GHI xuống thiết bị nhớ */
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE; /* Mã thao tác ghi */
  regs.a2 = phyaddr;         /* Địa chỉ vật lý đích */
  regs.a3 = value;           /* Giá trị cần ghi */
  
  _syscall(caller->krnl, caller->pid, 17, &regs);

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);

  // struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  /* TODO Invalid memory identify */

  pg_getval(caller->krnl->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    addr_t offset,      // Source address = [source] + [offset]
    uint32_t *destination)
{
  BYTE data;
  // printf("%s:%d\n", __func__, __LINE__);
  int val = __read(proc, 0, source, offset, &data);
  *destination = data;
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
  printf("%s:%d pid=%u pc=%u read reg=%u offset=%lu data=%u dest=%u\n",
         __func__,
         __LINE__,
         proc->pid,
         proc->pc,
         source,
         offset,
         (unsigned int)data,
         *destination);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  pg_setval(caller->krnl->mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
  printf("%s:%d pid=%u pc=%u write data=%u reg=%u offset=%lu\n",
         __func__,
         __LINE__,
         proc->pid,
         proc->pc,
         (unsigned int)data,
         destination,
         offset);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  return val;
}

/*libkmem_malloc- alloc region memory in kmem
 *@caller: caller
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 */

int libkmem_malloc(struct pcb_t *caller, uint32_t size, uint32_t reg_index)
{
  /* TODO: provide OS level management
   *       and forward the request to helper
   */
  // addr_t  addr;
  // int val = __kmalloc(caller, -1, reg_index, size, &addr);

  /* TODO: provide OS kmem allocation validation
   */

  return 0;
}

/*kmalloc - alloc region memory in kmem
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 *@alloc_addr: allocated address
 */
addr_t __kmalloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /* 1. Làm tròn kích thước theo đơn vị trang */
#ifdef MM64
  addr_t req_size = PAGING64_PAGE_ALIGNSZ(size);
  int incpgnum = req_size / PAGING64_PAGESZ;
  addr_t astart = 0xFFFFFFFF80000000; // Ranh giới Kernel Space 64-bit
  addr_t aend   = 0xFFFFFFFFFFFFFFFF;
#else
  addr_t req_size = PAGING_PAGE_ALIGNSZ(size);
  int incpgnum = req_size / PAGING_PAGESZ;
  addr_t astart = 0xC0000000;         // Ranh giới Kernel Space 32-bit
  addr_t aend   = 0xFFFFFFFF;
#endif

  struct vm_rg_struct newrg;
  
  /* Lấy VMA của Kernel (Giả định VMA ID = 0 là không gian Kernel) */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL) return -1;
  
  /* Sử dụng sbrk làm địa chỉ khởi đầu cấp phát cho Kernel */
  addr_t mapstart = cur_vma->sbrk;

  /* 2. TODO: vm_map_kernel
   * Map vùng nhớ ảo cho Kernel, yêu cầu RAM vật lý liên tục */
  if (vm_map_kernel(caller, astart, aend, mapstart, incpgnum, &newrg) < 0) {
      return -1; /* Lỗi: Không thể cấp phát (hết RAM hoặc vùng nhớ) */
  }
  
  /* Tịnh tiến đỉnh sbrk của VMA */
  cur_vma->sbrk += req_size;
  cur_vma->vm_end += req_size;

  /* 3. Đăng ký thông tin vào Symbol Region Table */
  *alloc_addr = newrg.rg_start;
  
  // TODO: symrgtbl .rg_start .rg_end .mode_bit
  /* Ghi nhận vùng nhớ vào bảng ký hiệu. 
   * mode_bit = 0 đánh dấu đây là vùng nhớ thuộc Kernel mode */
  caller->krnl->mm->symrgtbl[rgid].rg_start = newrg.rg_start;
  caller->krnl->mm->symrgtbl[rgid].rg_end   = newrg.rg_start + size; // Lưu kích thước thực tế
  caller->krnl->mm->symrgtbl[rgid].mode_bit = 0; // Kernel mode = 0

  return 0;
}

/*libkmem_cache_pool_create - create cache pool in kmem
 *@caller: caller
 *@size: memory size
 *@align: alignment size of each cache slot (identical cache slot size)
 *@cache_pool_id: cache pool ID
 */
int libkmem_cache_pool_create(struct pcb_t *caller, uint32_t size, uint32_t align, uint32_t cache_pool_id)
{
  /* 1. Tính toán kích thước trang cần thiết */
#ifdef MM64
  addr_t req_size = PAGING64_PAGE_ALIGNSZ(size);
  int incpgnum = req_size / PAGING64_PAGESZ;
  addr_t astart = 0xFFFFFFFF80000000; 
  addr_t aend   = 0xFFFFFFFFFFFFFFFF;
#else
  addr_t req_size = PAGING_PAGE_ALIGNSZ(size);
  int incpgnum = req_size / PAGING_PAGESZ;
  addr_t astart = 0xC0000000;
  addr_t aend   = 0xFFFFFFFF;
#endif

  struct vm_rg_struct newrg;
  
  /* 2. Quản lý địa chỉ ảo của Kernel (Bump Pointer) */
  /* Dùng chung một con trỏ static để cấp phát tịnh tiến cho toàn bộ Kernel */
  static addr_t next_kernel_vaddr = 0; 
  
  if (next_kernel_vaddr == 0) {
      next_kernel_vaddr = astart;
  }
  
  addr_t mapstart = next_kernel_vaddr;

  /* 3. Gọi vm_map_kernel để xin RAM liên tục */
  if (vm_map_kernel(caller, astart, aend, mapstart, incpgnum, &newrg) < 0) {
      return -1; /* Lỗi hết RAM hoặc phân mảnh */
  }

  /* Tịnh tiến con trỏ địa chỉ Kernel lên để dành cho lần cấp phát sau */
  next_kernel_vaddr = newrg.rg_end;

  /* 4. Khởi tạo mảng quản lý Pool nếu chưa tồn tại */
  if (caller->krnl->mm->kcpooltbl == NULL) {
      /* Khởi tạo một mảng gồm 10 pool (có thể tự chỉnh kích thước) */
      caller->krnl->mm->kcpooltbl = malloc(10 * sizeof(struct kcache_pool_struct));
  }

  /* 5. Lưu thông tin vùng nhớ vào struct kcache_pool_struct */
  caller->krnl->mm->kcpooltbl[cache_pool_id].size = size;
  caller->krnl->mm->kcpooltbl[cache_pool_id].align = align;
  caller->krnl->mm->kcpooltbl[cache_pool_id].storage = newrg.rg_start; 

  return 0;
}

/*libkmem_cache_alloc - allocate cache slot in cache pool, cache slot has identical size
 * the allocated size is embedded in pool management mechanism
 *@caller: caller
 *@cache_pool_id: cache pool ID
 *@reg_index: memory region index
 */
int libkmem_cache_alloc(struct pcb_t *proc, uint32_t cache_pool_id, uint32_t reg_index)
{
  /* TODO: provide OS level management
   *       and forward the request to helper
   */
  addr_t addr = __kmem_cache_alloc(proc, -1, reg_index, cache_pool_id, &addr);

  // krnl->kcpooltbl...
  // krnl->krnl_pgd ...

  return 0;
}

/*kmem_cache_alloc - alloc region memory in kmem cache
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@cache_pool_id: cached pool ID
 *@alloc_addr: allocated address
 */

addr_t __kmem_cache_alloc(struct pcb_t *caller, int vmaid, int rgid, int cache_pool_id, addr_t *alloc_addr)
{
  struct krnl_t *krnl = caller->krnl;

  /* Kiểm tra an toàn: Bảng pool đã được tạo chưa? */
  if (krnl->mm->kcpooltbl == NULL) {
      return -1; 
  }

  /* TODO: Trỏ tới cấu trúc Pool trong bảng */
  struct kcache_pool_struct *pool = &krnl->mm->kcpooltbl[cache_pool_id];
  
  /* Kiểm tra an toàn: Pool này có tồn tại (đã được create) không? */
  if (pool->storage == 0 || pool->align == 0) {
      return -1;
  }

  /* Cấp phát 1 Slot trống cho biến. 
   * Ở đây sử dụng chiến thuật cấp phát tịnh tiến đơn giản. 
   * Cần có biến cursor (như đã nói ở trên) để biết vị trí trống tiếp theo. */
  
  // *alloc_addr = pool->storage + pool->cursor;
  // pool->cursor += pool->align; 
  
  /* Nếu struct của bạn không có biến cursor, 
   * tạm thời cứ gán vào địa chỉ gốc theo đúng dummy code của bạn: */
  *alloc_addr = pool->storage;

  /* TODO: Cập nhật bảng quản lý biến (symrgtbl) của tiến trình */
  /* Ghi nhận vùng nhớ cấp phát vào ID biến (rgid) */
  krnl->mm->symrgtbl[rgid].rg_start = *alloc_addr;
  
  /* Kích thước của biến chính bằng kích thước align của slot */
  krnl->mm->symrgtbl[rgid].rg_end = *alloc_addr + pool->align;
  
  /* Đánh dấu đây là vùng nhớ Kernel (mode_bit = 0) */
  krnl->mm->symrgtbl[rgid].mode_bit = 0;

  return 0;
}

int libkmem_copy_from_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  BYTE data;
  /* Lặp qua từng byte để copy từ vùng nhớ User sang vùng nhớ Kernel */
  for (uint32_t i = 0; i < size; i++) {
    
    /* 1. Đọc 1 byte từ phân vùng của User Mode */
    if (__read_user_mem(caller, 0, source, offset + i, &data) != 0) {
        return -1; /* Lỗi đọc: Có thể biến User không tồn tại hoặc lỗi phân trang */
    }
    
    /* 2. Ghi byte đó vào phân vùng của Kernel Mode */
    if (__write_kernel_mem(caller, 0, destination, offset + i, data) != 0) {
        return -1; /* Lỗi ghi: Không có quyền hoặc tràn viền Kernel */
    }
    
  }
  return 0;
}

int libkmem_copy_to_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  BYTE data;
  /* Lặp qua từng byte để copy từ vùng nhớ Kernel trả về vùng nhớ User */
  for (uint32_t i = 0; i < size; i++) {
    
    /* 1. Đọc 1 byte từ phân vùng của Kernel Mode */
    if (__read_kernel_mem(caller, 0, source, offset + i, &data) != 0) {
        return -1; /* Lỗi đọc: Kernel không có dữ liệu tại vị trí này */
    }
    
    /* 2. Ghi byte đó vào phân vùng của User Mode */
    if (__write_user_mem(caller, 0, destination, offset + i, data) != 0) {
        return -1; /* Lỗi ghi: User không tồn tại biến hoặc offset bị tràn */
    }
    
  }
  return 0;
}

/*__read_kernel_mem - read value in kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  /* Lấy thông tin vùng nhớ của biến từ Symbol Region Table */
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
  
  /* Kiểm tra an toàn: Biến có tồn tại không? */
  if (currg == NULL) {
      return -1; 
  }

  addr_t kva = currg->rg_start + offset;
  
  /* Kiểm tra an toàn: Offset có vượt quá kích thước của biến không? */
  if (kva >= currg->rg_end) {
      return -1; 
  }

  int pgn, off;
  uint32_t pte;

  /* Phân giải địa chỉ ảo Kernel (KVA) ra Page Number và Offset */
#ifdef MM64
  pgn = kva / PAGING64_PAGESZ;
  off = kva % PAGING64_PAGESZ;

  addr_t pgd_i, p4d_i, pud_i, pmd_i, pt_i;
  /* Phân rã địa chỉ theo mô phỏng đa cấp */
  get_pd_from_pagenum(pgn, &pgd_i, &p4d_i, &pud_i, &pmd_i, &pt_i);
  
  /* Lấy Page Table Entry trực tiếp từ bảng trang của Kernel */
  pte = caller->krnl->krnl_pt[pgn];
#else
  pgn = PAGING_PGN(kva);
  off = PAGING_OFFST(kva);
  pte = caller->krnl->krnl_pgd[pgn];
#endif

  /* Kiểm tra xem trang có trên RAM không (Kernel page thường luôn có mặt) */
  if (!PAGING_PAGE_PRESENT(pte)) {
      return -1; /* Lỗi: Vùng nhớ Kernel chưa được ánh xạ */
  }

  /* Trích xuất số khung RAM vật lý (Frame Number) từ PTE */
  int fpn = PAGING_FPN(pte);

  /* Tính toán địa chỉ vật lý thật sự trên thanh RAM */
  int phyaddr;
#ifdef MM64
  phyaddr = (fpn * PAGING64_PAGESZ) + off;
#else
  phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif

  /* Hạt nhân gọi thẳng thiết bị phần cứng để đọc dữ liệu */
  MEMPHY_read(caller->krnl->mram, phyaddr, data);
  
  return 0;
}

/*__write_kernel_mem - write a kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
  
  if (currg == NULL) {
      return -1; 
  }

  addr_t kva = currg->rg_start + offset;
  
  if (kva >= currg->rg_end) {
      return -1; 
  }

  int pgn, off;
  uint32_t pte;

#ifdef MM64
  pgn = kva / PAGING64_PAGESZ;
  off = kva % PAGING64_PAGESZ;

  addr_t pgd_i, p4d_i, pud_i, pmd_i, pt_i;
  get_pd_from_pagenum(pgn, &pgd_i, &p4d_i, &pud_i, &pmd_i, &pt_i);
  
  pte = caller->krnl->krnl_pt[pgn];
#else
  pgn = PAGING_PGN(kva);
  off = PAGING_OFFST(kva);
  pte = caller->krnl->krnl_pgd[pgn];
#endif

  if (!PAGING_PAGE_PRESENT(pte)) {
      return -1; 
  }

  int fpn = PAGING_FPN(pte);
  int phyaddr;
  
#ifdef MM64
  phyaddr = (fpn * PAGING64_PAGESZ) + off;
#else
  phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif

  /* Hạt nhân gọi thẳng thiết bị phần cứng để ghi dữ liệu */
  MEMPHY_write(caller->krnl->mram, phyaddr, value);
  
  return 0;
}

/*__read_user_mem - read value in user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  /* TODO: provide OS level management user memory access */
  struct mm_struct *mm = caller->krnl->mm;
  
  /* Lấy hồ sơ quản lý của biến từ Symbol Region Table (symrgtbl) */
  struct vm_rg_struct *currg = &mm->symrgtbl[rgid];

  /* 1. Kiểm tra an toàn (Defensive Programming) */
  if (currg->rg_start == 0 && currg->rg_end == 0) {
      return -1; /* Lỗi: Biến chưa từng được cấp phát (ALLOC) */
  }
  
  addr_t vaddr = currg->rg_start + offset;
  
  if (vaddr >= currg->rg_end) {
      return -1; /* Lỗi: Truy cập vượt quá kích thước của biến (Out of bounds) */
  }

  /* 2. TODO: pg_getval tương tự read thôi */
  /* Gọi hàm pg_getval để lấy dữ liệu từ địa chỉ ảo vaddr, kết quả lưu vào *data */
  return pg_getval(mm, vaddr, data, caller);
}

/*__write_user_mem - write a user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
struct mm_struct *mm = caller->krnl->mm;
  
  /* Lấy hồ sơ quản lý của biến */
  struct vm_rg_struct *currg = &mm->symrgtbl[rgid];

  /* 1. Kiểm tra an toàn */
  if (currg->rg_start == 0 && currg->rg_end == 0) {
      return -1; /* Lỗi: Biến chưa tồn tại */
  }
  
  addr_t vaddr = currg->rg_start + offset;
  
  if (vaddr >= currg->rg_end) {
      return -1; /* Lỗi tràn viền */
  }

  /* (Tuỳ chọn) Chặn truy cập nếu vùng nhớ này bị khoá ở chế độ Kernel 
   * Nếu trước đó ở __kmalloc bạn set mode_bit = 0, thì lệnh ghi từ User sẽ bị từ chối */
  // if (currg->mode_bit == 0) return -1; 

  /* 2. TODO: pg_setval tương tự write thôi */
  /* Gọi hàm pg_setval để ghi giá trị 'value' xuống địa chỉ ảo vaddr */
  return pg_setval(mm, vaddr, value, caller);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->krnl->mm->pgd[pagenum];

    if (PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (!pg)
  {
    return -1;
  }
  *retpgn = pg->pgn;
  mm->fifo_pgn = pg->pg_next;
  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  struct vm_rg_struct *prev = NULL; /* Thêm con trỏ theo vết */

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;
      rgit->rg_start = rgit->rg_start + size;
      break; /* Tìm thấy và cấp phát xong, thoát vòng lặp */
    }
    else
    {
      prev = rgit;          /* Cập nhật prev trước khi đi tiếp */
      rgit = rgit->rg_next; /* Traverse next rg */
    }
  }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

// #endif
