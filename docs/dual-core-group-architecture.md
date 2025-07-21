# 双核心组架构详细设计文档

## 🎯 架构概述

本文档详细描述了 mem_shared 编译器扩展的双核心组架构设计，这是对原有单核心架构的重大升级。

### 核心特性
- **54个核心组**：每个组包含2个运算核心
- **108个总核心**：54组 × 2核心/组 = 108个核心
- **智能地址编码**：支持跨核心组的高效内存访问
- **灵活配置**：可配置核心数量和内存大小
- **透明访问**：编译器自动处理地址变换

## 🏗️ 系统架构

### 硬件组织结构
```
系统架构
├── 核心组 0
│   ├── 核心 0 (2KB local memory)
│   └── 核心 1 (2KB local memory)
├── 核心组 1
│   ├── 核心 0 (2KB local memory)  
│   └── 核心 1 (2KB local memory)
├── ...
└── 核心组 53
    ├── 核心 0 (2KB local memory)
    └── 核心 1 (2KB local memory)
```

### 内存配置
| 参数 | 默认值 | 最大值 | 配置选项 |
|------|--------|--------|----------|
| 核心组数 | 54 | 54 | `-fmem-shared-core-num=N` |
| 每组核心数 | 2 | 2 | 固定值 |
| 总核心数 | 108 | 108 | 54×2 |
| 每核心内存 | 2KB | 20KB | `-fmem-shared-core-size=S` |
| 总共享内存 | 216KB | 2160KB | 108×20KB |

## 📍 地址编码方案

### 地址变换规则

#### 1. 本核心访问
- **条件**：访问本核心的 local memory
- **地址变换**：不需要变换，直接使用原始地址
- **性能**：最优，无额外开销

#### 2. 跨核心访问
- **条件**：访问其他核心的 local memory  
- **地址变换**：必须进行特殊编码
- **性能**：轻微开销，硬件路由

### 跨核心地址编码格式

```
位布局：
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 X  X  C  X  R  G  G  G  G  G  G  I  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L  L

其中：
C  (bit 29)     : 跨核心访问标志位 (1=跨核心访问)
R  (bit 27)     : 保留位 (始终为0)
G  (bits 26:21) : 核心组号 (0-53，6位可表示0-63)
I  (bit 20)     : 组内ID号 (0或1，标识组内哪个核心)
L  (bits 19:0)  : 本地偏移量 (核心内的实际偏移地址)
```

### 地址编码示例

#### 示例1：访问组0核心0的偏移0x100
```
原始偏移: 0x100
编码结果: 0x20000100
解析：
- bit 29 = 1 (跨核心访问)
- bit 27 = 0 (保留位)  
- bits 26:21 = 0 (组号0)
- bit 20 = 0 (组内核心0)
- bits 19:0 = 0x100 (本地偏移)
```

#### 示例2：访问组25核心1的偏移0x800
```
原始偏移: 0x800
编码结果: 0x23300800
解析：
- bit 29 = 1 (跨核心访问)
- bit 27 = 0 (保留位)
- bits 26:21 = 25 (组号25 = 0x19)
- bit 20 = 1 (组内核心1)
- bits 19:0 = 0x800 (本地偏移)
```

#### 示例3：访问组53核心0的偏移0x1FF
```
原始偏移: 0x1FF
编码结果: 0x26A001FF
解析：
- bit 29 = 1 (跨核心访问)
- bit 27 = 0 (保留位)
- bits 26:21 = 53 (组号53 = 0x35)
- bit 20 = 0 (组内核心0)
- bits 19:0 = 0x1FF (本地偏移)
```

## 🧠 内存分配策略

### 三层分配架构

#### 1. 基础类型分配（MEM_SHARED_BASIC_TYPE）
- **策略**：轮询分配（Round-Robin）
- **目标**：负载均衡
- **适用**：int, float, double, char, 指针等基础类型

```c
// 轮询分配算法
unsigned int mem_shared_get_next_group(void) {
    unsigned int group = next_group_allocation;
    next_group_allocation = (next_group_allocation + 1) % mem_shared_num_groups;
    return group;
}
```

#### 2. 小数据分配（MEM_SHARED_SMALL_DATA）
- **策略**：最佳适配算法（Best-Fit）
- **目标**：空间优化
- **适用**：< 10KB的数组和结构体

```c
// 最佳适配算法
unsigned int mem_shared_get_best_group(unsigned int size) {
    unsigned int best_group = 0;
    unsigned int best_fit_size = UINT_MAX;
    
    for (i = 0; i < mem_shared_num_groups; i++) {
        for (j = 0; j < CORES_PER_GROUP; j++) {
            if (mem_shared_group_info[i].free_memory[j] >= size &&
                mem_shared_group_info[i].free_memory[j] < best_fit_size) {
                best_group = i;
                best_fit_size = mem_shared_group_info[i].free_memory[j];
            }
        }
    }
    return best_group;
}
```

#### 3. 大数据分配（MEM_SHARED_LARGE_DATA）
- **策略**：分布式存储
- **目标**：并行访问优化
- **适用**：≥ 10KB的大型数组

```c
// 分布式分配算法
void mem_shared_distribute_data(mem_shared_info_t *info, unsigned int total_size) {
    unsigned int chunk_size = (total_size + mem_shared_num_groups - 1) / mem_shared_num_groups;
    
    info->is_distributed = true;
    info->num_chunks = mem_shared_num_groups;
    info->chunk_size = chunk_size;
    info->target_group = 0;  // 从组0开始
    info->intra_group_id = 0; // 默认使用每组的第一个核心
}
```

### 分配决策流程

```
输入: mem_shared 变量声明
         ↓
    数据类型分析
         ↓
┌────────────────┬────────────────┬────────────────┐
│ 基础类型？     │ 小数据？       │ 大数据？       │
│ int,float,etc  │ size < 10KB    │ size ≥ 10KB    │
└────────────────┴────────────────┴────────────────┘
         ↓                ↓                ↓
    轮询分配          最佳适配         分布式存储
         ↓                ↓                ↓
    组N核心M          组X核心Y         跨所有组
         ↓                ↓                ↓
    单一地址          单一地址         多个地址
```

## 🔧 编译器选项

### 核心配置选项

#### `-fmem-shared`
- **功能**：启用 mem_shared 关键字支持
- **默认**：禁用
- **示例**：`gcc -fmem-shared program.c`

#### `-fmem-shared-core-num=N`
- **功能**：指定参与共享的核心数量
- **默认**：108（54组×2核心）
- **范围**：2-108（必须是偶数，因为每组2个核心）
- **示例**：`gcc -fmem-shared -fmem-shared-core-num=72 program.c`

#### `-fmem-shared-core-size=S`
- **功能**：指定每个核心的共享内存大小（字节）
- **默认**：2048（2KB）
- **范围**：1024-20480（1KB-20KB）
- **示例**：`gcc -fmem-shared -fmem-shared-core-size=8192 program.c`

#### `-fdump-mem-shared`
- **功能**：启用详细的分配调试输出
- **默认**：禁用
- **输出**：分配信息、地址编码、内存使用统计
- **示例**：`gcc -fmem-shared -fdump-mem-shared program.c`

### 配置组合示例

#### 高密度配置
```bash
gcc -fmem-shared -fmem-shared-core-num=108 -fmem-shared-core-size=20480 program.c
# 使用所有108个核心，每核心20KB，总计2160KB共享内存
```

#### 平衡配置
```bash
gcc -fmem-shared -fmem-shared-core-num=54 -fmem-shared-core-size=4096 program.c
# 使用27组54个核心，每核心4KB，总计216KB共享内存
```

#### 小型配置
```bash
gcc -fmem-shared -fmem-shared-core-num=16 -fmem-shared-core-size=1024 program.c
# 使用8组16个核心，每核心1KB，总计16KB共享内存
```

## 🎛️ 内置函数增强

### 分布式内置函数处理

#### 原理
当使用 `memset`/`memcpy` 等内置函数操作分布式数据时，编译器自动将单个调用替换为多个分块调用。

#### 示例转换

##### memset操作
```c
// 用户代码
mem_shared int large_array[8192];  // 32KB，分布到54组
memset(large_array, 0, sizeof(large_array));

// 编译器生成（简化显示）
memset((void*)0x20000000, 0, 607);  // 组0，607字节
memset((void*)0x20400000, 0, 607);  // 组1，607字节
// ... 共54个调用
memset((void*)0x26A00000, 0, 607);  // 组53，607字节
```

##### memcpy操作
```c
// 用户代码  
mem_shared int src_array[8192];
mem_shared int dst_array[8192];
memcpy(dst_array, src_array, sizeof(src_array));

// 编译器生成（组对组复制）
memcpy((void*)0x20000000, (void*)0x20001000, 607);  // 组0→组0
memcpy((void*)0x20400000, (void*)0x20401000, 607);  // 组1→组1
// ... 共54个调用
```

### 性能影响分析

| 操作类型 | 调用次数 | 性能影响 | 适用场景 |
|----------|----------|----------|----------|
| 单组→单组 | 1 | 无影响 | 小数据操作 |
| 常规→单组 | 1 | 无影响 | 初始化 |
| 单组→常规 | 1 | 无影响 | 结果收集 |
| 常规→分布 | 54 | 中等影响 | 数据分发 |
| 分布→常规 | 54 | 中等影响 | 数据收集 |
| 分布→分布 | 54 | 较大影响 | 大数据处理 |

## 🔍 调试和诊断

### 调试输出格式

#### 初始化信息
```
[mem_shared] Initialized: 54 groups (108 cores), 2048 bytes per core
```

#### 分配信息
```
[mem_shared] Allocated 4 bytes for 'counter' in group 0 core 0 at offset 0x0
[mem_shared] Allocated 2048 bytes for 'buffer' in group 5 core 1 at offset 0x400
[mem_shared] Distributed 32768 bytes across 54 groups, 607 bytes per chunk
```

#### 访问操作
```
[mem_shared] Load from 'counter' group 0 intra 0 offset 0x0
[mem_shared] Store to 'buffer' group 5 intra 1 offset 0x400
```

#### 内置函数替换
```
[mem_shared] Replaced memset call (regular to mem_shared) with 54 chunk operations
[mem_shared] Generated memset for group 0, intra 0, offset 0x0, size 607
[mem_shared] Generated memset for group 1, intra 0, offset 0x0, size 607
```

### 内存使用统计

#### 组使用情况
```
=== mem_shared Group Usage ===
Group  Core0-Used  Core0-Free  Core1-Used  Core1-Free  Variables
    0        1024        1024           0        2048          2
    1         512        1536           0        2048          1
    2           0        2048        1024        1024          1
   ...
Total: 45056 bytes used, 171008 bytes available
Usage: 20.8% of total capacity
```

## 🚀 性能优化

### 访问模式优化

#### 本地访问优化
- **检测**：编译器检测访问的变量是否在当前核心
- **优化**：本地访问跳过地址编码
- **收益**：零开销的本地内存访问

#### 组内访问优化
- **检测**：访问同组内的另一个核心
- **优化**：简化地址编码路径
- **收益**：减少地址计算开销

#### 分布式访问优化
- **检测**：大数据的连续访问模式
- **优化**：预取和批处理
- **收益**：提高缓存利用率

### 编译时优化

#### 常量折叠
```c
// 编译时已知的地址计算会被优化
mem_shared int var;
int *ptr = &var;  // 地址在编译时计算并常量化
```

#### 循环优化
```c
// 编译器识别分布式数组的访问模式并优化
mem_shared int array[8192];
for (int i = 0; i < 8192; i++) {
    array[i] = i;  // 编译器可能生成向量化代码
}
```

## 🧪 测试和验证

### 功能测试

#### 单元测试覆盖
- ✅ 基础类型分配
- ✅ 小数据最佳适配
- ✅ 大数据分布式存储
- ✅ 地址编码正确性
- ✅ 内置函数替换
- ✅ 跨组访问

#### 集成测试
- ✅ 多种数据类型混合
- ✅ 复杂的内存访问模式
- ✅ 大规模分布式操作
- ✅ 边界条件处理

#### 性能测试
- ✅ 本地访问性能
- ✅ 跨组访问延迟
- ✅ 分布式操作吞吐量
- ✅ 内存使用效率

### 回归测试

#### 兼容性测试
- ✅ 不同核心配置
- ✅ 不同内存大小
- ✅ 边界配置组合
- ✅ 向后兼容性

## 📈 未来发展

### 近期规划
- [ ] 智能本地访问检测
- [ ] 组内访问优化
- [ ] 并行内置函数调用
- [ ] DMA集成支持

### 长期目标
- [ ] 自适应分配策略
- [ ] 硬件感知优化
- [ ] 跨组通信优化
- [ ] 实时负载平衡

## 📚 总结

双核心组架构为 mem_shared 提供了：

### 技术优势
- **扩展性**：支持最多108个核心的大规模系统
- **灵活性**：可配置的核心数量和内存大小
- **效率性**：智能的分配策略和地址编码
- **透明性**：用户无需了解底层复杂性

### 应用价值
- **高性能计算**：大规模并行数据处理
- **嵌入式系统**：多核心资源高效利用
- **科学计算**：分布式矩阵和数组运算
- **图像处理**：大图像数据的并行处理

这个架构为众核心编程提供了一个强大、灵活且易用的编程模型，显著降低了多核心编程的复杂性，同时保持了高性能的访问特性。