# TASK BTL - Simple OS Assignment Summary


## Phần Scheduler hiện thực `queue.c` and `shed.c` (lý thuyết xem ở task 1)

### Chú ý 2 CPU có thể khác nhau nhưng 1 CPU thì sẽ gần như giống nhau
```sh

# mở MM_FIXED_MEMSZ trong file os-cfg.h để tránh lỗi 
#define MM_FIXED_MEMSZ

## BUILD
make

## RUN
./os votien_sched_1 >  result_sched/votien_sched_1.text  
python3 gantt.py input/votien_sched_1 result_sched/votien_sched_1.text result_sched/votien_sched_1.png

./os votien_sched_2 >  result_sched/votien_sched_2.text  
python3 gantt.py input/votien_sched_2 result_sched/votien_sched_2.text result_sched/votien_sched_2.png

./os votien_sched_3 >  result_sched/votien_sched_3.text  
python3 gantt.py input/votien_sched_3 result_sched/votien_sched_3.text result_sched/votien_sched_3.png

./os votien_sched_4 >  result_sched/votien_sched_4.text  
python3 gantt.py input/votien_sched_4 result_sched/votien_sched_4.text result_sched/votien_sched_4.png

./os votien_sched_5 >  result_sched/votien_sched_5.text  
python3 gantt.py input/votien_sched_5 result_sched/votien_sched_5.text result_sched/votien_sched_5.png

./os sched >  result_sched/sched.text  
python3 gantt.py input/sched result_sched/sched.text result_sched/sched.png

./os sched_0 >  result_sched/sched_0.text  
python3 gantt.py input/sched_0 result_sched/sched_0.text result_sched/sched_0.png

./os sched_1 >  result_sched/sched_1.text  
python3 gantt.py input/sched_1 result_sched/sched_1.text result_sched/sched_1.png
```

## Phần 1 MEMORY USER hiện thực `libmem.c`, `sys_mem.c`, `mm64.c` and `mm-vm.c` (lý thuyết xem ở task 2)


```sh
## USE #define DEBUG_PRINT 1 trong file os-cfg.h
#define DEBUG_PRINT 1
// #define IODUMP 1
# ALLOC NO SWAPING
./os votien_memory_1 >  result_memory/votien_memory_1.text 

# ALLOC SWAPING
./os votien_memory_2 >  result_memory/votien_memory_2.text

# WRITE AND READ NO SWAPING
 ./os votien_memory_3 >  result_memory/votien_memory_3.text

# WRITE AND READ SWAPING
./os votien_memory_4 >  result_memory/votien_memory_4.text
./os votien_memory_5 >  result_memory/votien_memory_5.text

# FREE
 ./os votien_memory_6 >  result_memory/votien_memory_6.text

# KMALLOC
./os votien_memory_7 >  result_memory/votien_memory_7.text

# copy_from_user + copy_to_user
./os votien_memory_8 >  result_memory/votien_memory_8.text

# kmem_cache_create + kmem_cache_alloc
./os votien_memory_8 >  result_memory/votien_memory_8.text

## USE #define IODUMP 1
// #define DEBUG_PRINT 1
#define IODUMP 1
./os os_0_mlq_paging >  result_memory/os_0_mlq_paging.text
./os os_1_mlq_paging >  result_memory/os_1_mlq_paging.text
./os os_1_singleCPU_mlq_paging >  result_memory/os_1_singleCPU_mlq_paging.text
```

## Phần 2 MEMORY KERNEL hiện thực `libmem.c`, `sys_mem.c`, `mm64.c` and `mm-vm.c` (lý thuyết xem trong discord)

```
# KMALLOC
./os votien_memory_7 >  result_memory/votien_memory_7.text

# copy_from_user + copy_to_user
./os votien_memory_8 >  result_memory/votien_memory_8.text

# kmem_cache_create + kmem_cache_alloc
./os votien_memory_8 >  result_memory/votien_memory_8.text
```

---


<p align="center">
  <a href="https://www.facebook.com/Shiba.Vo.Tien">
    <img src="https://img.shields.io/badge/Facebook-1877F2?style=for-the-badge&logo=facebook&logoColor=white" alt="Facebook"/>
  </a>
  <a href="https://www.tiktok.com/@votien_shiba">
    <img src="https://img.shields.io/badge/TikTok-000000?style=for-the-badge&logo=tiktok&logoColor=white" alt="TikTok"/>
  </a>
  <a href="https://www.facebook.com/groups/khmt.ktmt.cse.bku?locale=vi_VN">
    <img src="https://img.shields.io/badge/Facebook%20Group-4267B2?style=for-the-badge&logo=facebook&logoColor=white" alt="Facebook Group"/>
  </a>
  <a href="https://www.facebook.com/CODE.MT.BK">
    <img src="https://img.shields.io/badge/Page%20CODE.MT.BK-0057FF?style=for-the-badge&logo=facebook&logoColor=white" alt="Facebook Page"/>
  </a>
  <a href="https://github.com/VoTienBKU">
    <img src="https://img.shields.io/badge/GitHub-181717?style=for-the-badge&logo=github&logoColor=white" alt="GitHub"/>
  </a>
</p>
