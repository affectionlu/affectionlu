# 修正的Local Memory池设计

## 🎯 问题重新分析

### 原错误理解
我之前错误地认为需要为每个核心生成不同的静态池声明：

```c
// ❌ 错误的设计 - 每个核心不同的池名称
__attribute__((section(".llc"))) 
static char __mem_shared_pool_core_0_group_0_intra_0[2048];

__attribute__((section(".llc"))) 
static char __mem_shared_pool_core_1_group_0_intra_1[2048];
// ... 108个不同的池
```

这种设计的问题：
1. **违反了程序同构性**: 每个核心应该看到相同的程序代码
2. **无法实现**: 编译器无法知道代码运行在哪个核心上
3. **链接复杂**: 需要为每个核心生成不同的目标文件

### 正确理解 ✅

**核心观念**: 所有核心看到相同的程序，但每个核心有自己的local memory实例。

```c
// ✅ 正确的设计 - 所有核心看到相同的声明
__attribute__((section(".llc"))) 
static char __mem_shared_local_pool[2048];

/*
运行时视图：
- 核心0执行程序时：__mem_shared_local_pool 指向核心0的local memory (地址如 0x40000000)
- 核心1执行程序时：__mem_shared_local_pool 指向核心1的local memory (地址也是 0x40000000)
- 核心2执行程序时：__mem_shared_local_pool 指向核心2的local memory (地址也是 0x40000000)

跨核心访问时：
- 核心0访问核心1的池：encode_address(group=0, intra=1, 0x40000000)
- 核心0访问核心2的池：encode_address(group=1, intra=0, 0x40000000)
*/
```

## 🔧 修正后的技术架构

### 1. 数据结构重新设计

```c
/* 全局池描述符 - 编译器视角的单一池 */
typedef struct mem_shared_global_pool {
  /* 静态池管理 */
  tree static_pool_decl;            /* 单一池声明，所有核心可见 */
  uintptr_t local_pool_base;        /* 每个核心看到的本地地址（相同值） */
  size_t pool_size;                 /* 每个核心的池大小 */
  
  /* 按核心跟踪分配情况 */
  size_t used_bytes[MAX_TOTAL_CORES];    /* 每个核心已用字节数 */
  uintptr_t current_ptr[MAX_TOTAL_CORES]; /* 每个核心当前分配指针 */
  
  /* 全局状态 */
  bool initialized;
  bool dynamic_enabled;
} mem_shared_global_pool_t;
```

### 2. 地址编码机制

```c
uintptr_t mem_shared_get_pool_address_for_core(unsigned int core_id) {
    unsigned int group_id = core_id / CORES_PER_GROUP;
    unsigned int intra_id = core_id % CORES_PER_GROUP;
    
    /* 所有核心的本地池地址都是相同的（如0x40000000）*/
    uintptr_t local_addr = mem_shared_global_pool.local_pool_base;
    
    /* 当需要跨核心访问时，编码目标核心信息 */
    return mem_shared_encode_cross_core_address(group_id, intra_id, local_addr);
}
```

### 3. 池分配逻辑

```c
bool mem_shared_pool_allocate(unsigned int core_id, size_t size, uintptr_t *addr_out) {
    /* 计算目标核心在其本地池中的分配地址 */
    *addr_out = mem_shared_global_pool.local_pool_base + 
                mem_shared_global_pool.used_bytes[core_id];
    
    /* 更新该核心的分配跟踪 */
    mem_shared_global_pool.used_bytes[core_id] += size;
    
    return true;
}
```

## 📊 地址空间设计

### 本地访问视图（每个核心内部）
```
地址空间布局（每个核心相同）:
0x40000000  ┌─────────────────────┐
            │ __mem_shared_local_ │
            │ pool[0..2047]       │
0x400007FF  └─────────────────────┘
            │ 其他local memory    │
0x4007FFFF  └─────────────────────┘
```

### 跨核心访问视图（编码后）
```
编码地址格式:
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 X  X  1  X  0  G  G  G  G  G  G  I  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L

其中：
- 位29=1: 表示跨核心访问
- 位27=0: 保留位
- 位26:21=GGGGGG: 目标核心组号(0-53)
- 位20=I: 组内ID(0或1)
- 位19:0=LLLLLLLLLLLLLLLLLLLL: 目标核心的本地地址偏移
```

### 实际访问示例
```c
// 核心0访问自己的池
char *local_ptr = __mem_shared_local_pool + 100;  // 地址: 0x40000064

// 核心0访问核心1(group=0, intra=1)的池
char *remote_ptr = encode_address(0, 1, 0x40000064);  // 地址: 0x20100064

// 核心0访问核心54(group=27, intra=0)的池  
char *remote_ptr2 = encode_address(27, 0, 0x40000064); // 地址: 0x20D80064
```

## 🎪 代码生成示例

### 编译器生成的静态池
```c
/* 编译器为所有核心生成相同的声明 */
__attribute__((section(".llc"))) 
static char __mem_shared_local_pool[2048];
```

### mem_shared变量分配
```c
mem_shared int data[100];  // 400字节

/* 编译器分配逻辑：
1. 选择目标核心（如核心5）
2. 在核心5的跟踪中分配400字节
   - used_bytes[5] += 400
   - 本地偏移 = previous_used_bytes[5]
3. 生成访问代码时使用编码地址 */
```

### 跨核心访问代码生成
```c
// 源代码
mem_shared int distributed_array[2048];  // 分布到108个核心
int value = distributed_array[1000];

// 编译器生成（简化）
int target_core = calculate_target_core(1000);          // 如核心27
int local_offset = calculate_local_offset(1000);        // 如偏移200
uintptr_t encoded_addr = encode_address(
    target_core / 2,     // group_id = 13
    target_core % 2,     // intra_id = 1  
    0x40000000 + local_offset  // 目标核心的本地地址
);
int value = *(int*)encoded_addr;
```

## 🚀 优势

### 1. 符合硬件现实
- **程序同构**: 所有核心执行相同的程序代码
- **地址一致**: 每个核心看到的本地地址相同
- **硬件简单**: 地址编码由硬件MMU或路由器处理

### 2. 编译器简化
- **单一声明**: 只需生成一个静态池声明
- **统一管理**: 全局视角管理所有核心的分配
- **编码清晰**: 地址编码逻辑清晰明确

### 3. 运行时高效
- **本地访问**: 核心访问自己的内存无额外开销
- **跨核心访问**: 清晰的地址编码，硬件路由高效
- **内存对齐**: 保持良好的内存访问模式

## 🔍 与原设计的对比

| 方面 | 原错误设计 | 修正设计 |
|------|-----------|----------|
| 池声明数量 | 108个不同名称 | 1个相同声明 |
| 程序同构性 | 违反（每核心不同程序） | 符合（所有核心相同程序） |
| 地址一致性 | 不一致 | 一致（每核心看到相同本地地址） |
| 编译复杂度 | 高（需生成多个声明） | 低（单一声明） |
| 硬件支持 | 不现实 | 现实可行 |
| 调试便利性 | 困难 | 简单 |

## 💡 实现要点

1. **编译时**: 只生成一个`__mem_shared_local_pool`声明
2. **分配时**: 在全局跟踪表中记录每核心的使用情况
3. **访问时**: 使用地址编码指向目标核心的本地地址
4. **运行时**: 每个核心看到相同的程序和声明

这种设计完全符合多核系统的硬件现实，既保证了性能，又简化了编译器实现。