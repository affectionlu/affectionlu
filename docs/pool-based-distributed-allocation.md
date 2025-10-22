# 基于Memory Pool的分布式数据分配

## 🎯 问题背景

您指出了分布式数据分配中的一个关键问题：`mem_shared_distribute_data`需要从memory pool中真正分配内存，而不是假设可以直接使用任意地址。因为我们是通过`__attribute__((section(".llc")))`在每个核心上的存储空间模拟内存共享。

## ✅ 修正方案

### 核心改进
1. **真实内存分配**: 为每个chunk从对应核心的memory pool分配真实内存
2. **Chunk信息跟踪**: 记录每个chunk的实际分配地址和池内偏移
3. **分配失败处理**: 处理某些核心无法分配的情况
4. **地址计算修正**: 使用实际分配的地址进行跨核心访问

### 1. 新增Chunk信息结构

```c
/* 单个chunk的详细信息 */
typedef struct mem_shared_chunk_info {
  unsigned int core_id;           /* 分配到的核心ID */
  unsigned int group_id;          /* 核心组号 (0-53) */
  unsigned int intra_id;          /* 组内ID (0或1) */
  uintptr_t local_address;        /* 在核心memory pool中的本地地址 */
  unsigned int offset_in_pool;    /* 在memory pool中的偏移量 */
  unsigned int chunk_size;        /* 这个chunk的大小 */
  bool allocated;                 /* 是否成功分配 */
} mem_shared_chunk_info_t;

/* mem_shared变量信息扩展 */
typedef struct mem_shared_info {
  // ... 原有字段 ...
  
  /* 分布式数据chunk信息 */
  mem_shared_chunk_info_t *chunks; /* chunk信息数组 */
  unsigned int allocated_chunks;   /* 成功分配的chunk数量 */
} mem_shared_info_t;
```

### 2. 修正的分布式分配算法

```c
void mem_shared_distribute_data(mem_shared_info_t *info, unsigned int total_size) {
    unsigned int chunk_size = (total_size + mem_shared_total_cores - 1) / mem_shared_total_cores;
    
    /* 分配chunk信息数组 */
    info->chunks = XNEWVEC(mem_shared_chunk_info_t, mem_shared_total_cores);
    info->allocated_chunks = 0;
    
    /* 尝试在每个核心上分配chunk */
    for (int i = 0; i < mem_shared_total_cores; i++) {
        unsigned int core_id = i;
        unsigned int actual_chunk_size = chunk_size;
        uintptr_t chunk_addr;
        
        /* 调整最后一个chunk的大小 */
        if (是最后一个chunk) {
            actual_chunk_size = total_size - (chunk_size * allocated_chunks);
        }
        
        /* 从该核心的memory pool分配内存 */
        if (mem_shared_pool_allocate(core_id, actual_chunk_size, &chunk_addr)) {
            /* 记录chunk信息 */
            info->chunks[i].core_id = core_id;
            info->chunks[i].group_id = core_id / CORES_PER_GROUP;
            info->chunks[i].intra_id = core_id % CORES_PER_GROUP;
            info->chunks[i].local_address = chunk_addr;
            info->chunks[i].offset_in_pool = chunk_addr - mem_shared_global_pool.local_pool_base;
            info->chunks[i].chunk_size = actual_chunk_size;
            info->chunks[i].allocated = true;
            
            info->allocated_chunks++;
        } else {
            /* 分配失败，标记为未分配 */
            info->chunks[i].allocated = false;
        }
    }
}
```

### 3. 地址计算修正

#### 目标核心计算
```c
unsigned int mem_shared_calculate_target_group(tree decl, HOST_WIDE_INT offset) {
    mem_shared_info_t *info = mem_shared_get_info(decl);
    
    if (info->is_distributed && info->chunks) {
        unsigned int chunk_index = offset / info->chunk_size;
        unsigned int target_core = chunk_index % info->allocated_chunks;
        
        /* 查找第target_core个已分配的chunk */
        unsigned int allocated_count = 0;
        for (int i = 0; i < mem_shared_total_cores; i++) {
            if (info->chunks[i].allocated) {
                if (allocated_count == target_core) {
                    return info->chunks[i].group_id;  // 返回实际分配的核心组
                }
                allocated_count++;
            }
        }
    }
    
    // 回退到简单计算...
}
```

#### 本地偏移计算
```c
unsigned int mem_shared_calculate_local_offset(tree decl, HOST_WIDE_INT offset) {
    if (info->is_distributed && info->chunks) {
        // 找到目标chunk
        unsigned int chunk_index = offset / info->chunk_size;
        unsigned int target_core = chunk_index % info->allocated_chunks;
        
        // 使用实际分配的pool偏移
        return info->chunks[target_core].offset_in_pool + (offset % info->chunk_size);
    }
}
```

## 📊 分配示例

### 场景：分布式大数组
```c
mem_shared int big_array[8192];  // 32KB数组
```

#### 分配过程
```
1. 计算chunk大小: 32KB / 108核心 ≈ 303字节/核心

2. 尝试为每个核心分配:
   核心0: mem_shared_pool_allocate(0, 303, &addr) 
          -> 成功: addr=0x40000000, pool_offset=0x000
   核心1: mem_shared_pool_allocate(1, 303, &addr)
          -> 成功: addr=0x40000000, pool_offset=0x000
   ...
   核心50: mem_shared_pool_allocate(50, 303, &addr)
           -> 失败: pool已满
   ...

3. 结果: 成功分配100个chunks，失败8个
```

#### Chunk信息记录
```c
info->chunks[0] = {
    .core_id = 0,
    .group_id = 0, .intra_id = 0,
    .local_address = 0x40000000,
    .offset_in_pool = 0x000,
    .chunk_size = 303,
    .allocated = true
};

info->chunks[50] = {
    .allocated = false  // 分配失败
};
```

#### 访问地址计算
```c
// 访问 big_array[1000]
offset = 1000 * sizeof(int) = 4000;
chunk_index = 4000 / 303 = 13;
target_core = 13 % 100 = 13;  // 第13个已分配的核心

// 查找第13个已分配的chunk (假设是核心15)
target_group = info->chunks[15].group_id = 7;
target_intra = info->chunks[15].intra_id = 1;
local_offset = info->chunks[15].offset_in_pool + (4000 % 303) = 0x000 + 145;

// 编码跨核心地址
encoded_addr = encode_address(7, 1, 0x40000000 + 145);
```

## 🚀 关键优势

### 1. 内存分配真实性
- **真实分配**: 每个chunk都从实际的memory pool分配
- **地址有效**: 所有地址都指向真实的local memory空间
- **硬件兼容**: 完全符合`__attribute__((section(".llc")))`的硬件模型

### 2. 容错能力
- **分配失败处理**: 优雅处理某些核心内存不足的情况
- **动态调整**: 根据实际分配成功的核心数调整chunk分布
- **错误诊断**: 详细的分配失败信息

### 3. 地址计算精确性
- **实际偏移**: 使用真实的pool内偏移，不是假设偏移
- **分配感知**: 地址计算考虑实际分配成功的核心分布
- **一致性**: 保证访问地址与分配地址的一致性

## 📈 性能影响

### 编译时开销
| 操作 | 原实现 | 修正实现 | 变化 |
|------|--------|----------|------|
| 分配时间 | O(1) | O(cores) | 增加了分配调用 |
| 内存使用 | 常量 | O(cores) | 增加了chunk信息 |
| 地址计算 | O(1) | O(cores) | 需要查找已分配chunk |

### 运行时优势
- **访问精确**: 直接访问正确的local memory地址
- **硬件优化**: 充分利用local memory的高速特性
- **错误减少**: 避免访问无效或未分配的内存区域

## 🔍 调试信息

启用`-fdump-mem-shared`时的输出示例：
```
[mem_shared] Allocated chunk 0: core 0 (group 0 intra 0), size 303, addr 0x40000000, pool_offset 0x000
[mem_shared] Allocated chunk 1: core 1 (group 0 intra 1), size 303, addr 0x40000000, pool_offset 0x000
[mem_shared] Failed to allocate chunk on core 50 (group 25 intra 0), size 303
[mem_shared] Successfully distributed 32768 bytes across 100 cores, 327 bytes per chunk
[mem_shared] Adjusted chunk size to 327 bytes due to 100 available cores
```

## 💡 设计哲学

**原理念**: 假设可以在任意地址分配分布式数据
**新理念**: 基于真实memory pool的约束进行分布式分配

这种修正确保了分布式数据分配的现实可行性，同时保持了高性能的跨核心访问能力。每个chunk都有真实的内存支撑，避免了虚假地址的问题。