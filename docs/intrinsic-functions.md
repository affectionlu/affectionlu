# mem_shared 内置函数处理机制 - 完整版

## 概述

当用户对 `mem_shared` 变量使用 `memset`、`memcpy` 等内置函数时，编译器会自动检测并替换这些调用，生成适合分布式存储的多个函数调用。这确保了即使数据分布在不同核心上，这些操作也能正确执行。

**重要更新**：现在完全支持 `mem_shared` 变量作为源操作数（source operand）！

## 问题描述

### 原始问题
```c
mem_shared int data[1024];  // 假设分布存储

// 作为目标（原始支持）
memset(data, 1, 1024);      // 原始调用无法正确处理分布式数据
memcpy(data, source, 1024); // 同样的问题

// 作为源（新增支持）
memcpy(dest, data, 1024);   // 从分布式数据复制到其他位置
```

### 解决方案
编译器自动将单个内置函数调用替换为多个调用，每个调用针对特定的核心和内存段。

## 支持的操作类型

### 1. 目标操作（Destination Operations）- 原始支持
```c
mem_shared int target[1000];
int regular_source[1000];

// 常规内存 → mem_shared
memcpy(target, regular_source, sizeof(target));
memset(target, 0, sizeof(target));
```

### 2. 源操作（Source Operations）- 🆕 新增支持
```c
mem_shared int source[1000];
int regular_target[1000];

// mem_shared → 常规内存
memcpy(regular_target, source, sizeof(source));
```

### 3. 双向操作（Both Operations）- 🆕 新增支持
```c
mem_shared int source[1000];
mem_shared int target[1000];

// mem_shared → mem_shared
memcpy(target, source, sizeof(source));
```

## 支持的内置函数

### 内存操作函数
| 函数 | 目标支持 | 源支持 | 说明 |
|------|----------|--------|------|
| `memset` | ✅ 完全支持 | N/A | 自动分块设置内存值 |
| `memcpy` | ✅ 完全支持 | ✅ **新增支持** | 自动分块复制内存 |
| `memmove` | ✅ 完全支持 | ✅ **新增支持** | 支持重叠内存区域 |
| `memcmp` | ⚠️ 部分支持 | ✅ **新增支持** | 生成性能警告 |
| `bzero` | ✅ 完全支持 | N/A | 等价于 memset(..., 0, ...) |
| `bcopy` | ✅ 完全支持 | ✅ **新增支持** | 等价于 memcpy |

### 字符串操作函数
| 函数 | 目标支持 | 源支持 | 说明 |
|------|----------|--------|------|
| `strcpy` | 🔄 开发中 | 🔄 开发中 | 需要运行时长度计算 |
| `strncpy` | 🔄 开发中 | 🔄 开发中 | 固定长度，更容易处理 |
| `strlen` | N/A | 🔄 开发中 | 需要跨核心搜索 |
| `strcmp` | N/A | 🔄 开发中 | 需要跨核心比较 |
| `strncmp` | N/A | 🔄 开发中 | 固定长度比较 |

## 实现机制

### 1. 检测阶段
编译器在 GIMPLE 优化阶段插入专门的 pass 来检测内置函数调用：

```c
// 增强的检测流程
if (is_gimple_call(stmt)) {
    tree fndecl = gimple_call_fndecl(call);
    if (mem_shared_detect_intrinsic(fndecl) != UNKNOWN) {
        // 检查所有参数，不仅仅是第一个
        for (unsigned i = 0; i < gimple_call_num_args(call); i++) {
            tree arg = gimple_call_arg(call, i);
            if (extract_mem_shared_var(arg)) {
                // 发现需要处理的调用
                replace_intrinsic_call(call);
                break;
            }
        }
    }
}
```

### 2. 分析阶段
确定源和目标的分布情况：

```c
mem_shared_intrinsic_context_t ctx;
mem_shared_analyze_intrinsic_call(call_expr, &ctx);

// 分析结果包括：
// - 目标变量的分布情况 (target_is_distributed)
// - 源变量的分布情况 (source_is_distributed)
// - 操作大小和范围  
// - 需要生成的块操作数量
```

### 3. 代码生成阶段
根据源和目标的分布生成相应的函数调用：

#### 🆕 源为单核，目标为常规内存
```c
// 原始代码
mem_shared int small_array[256];  // 1KB，单核存储
int regular_buffer[256];
memcpy(regular_buffer, small_array, sizeof(small_array));

// 编译器生成
memcpy(regular_buffer, (void*)((2 << 21) | 0x800), 1024);
//     源地址编码：核心2，偏移0x800
```

#### 🆕 源为分布式，目标为常规内存
```c
// 原始代码
mem_shared int large_array[3000];  // 12KB，分布存储
int regular_buffer[3000];
memcpy(regular_buffer, large_array, sizeof(large_array));

// 编译器生成（4核系统）
memcpy(regular_buffer + 0,    (void*)((0 << 21) | 0x0000), 3000);  // 从核心0
memcpy(regular_buffer + 3000, (void*)((1 << 21) | 0x0000), 3000);  // 从核心1  
memcpy(regular_buffer + 6000, (void*)((2 << 21) | 0x0000), 3000);  // 从核心2
memcpy(regular_buffer + 9000, (void*)((3 << 21) | 0x0000), 3000);  // 从核心3
```

#### 🆕 源和目标都为mem_shared
```c
// 原始代码
mem_shared int source_array[3000];  // 12KB，分布存储
mem_shared int target_array[3000];  // 12KB，分布存储
memcpy(target_array, source_array, sizeof(source_array));

// 编译器生成（4核系统，核心对核心）
memcpy((void*)((0 << 21) | dst_off0), (void*)((0 << 21) | src_off0), 3000);
memcpy((void*)((1 << 21) | dst_off1), (void*)((1 << 21) | src_off1), 3000);
memcpy((void*)((2 << 21) | dst_off2), (void*)((2 << 21) | src_off2), 3000);
memcpy((void*)((3 << 21) | dst_off3), (void*)((3 << 21) | src_off3), 3000);
```

## 地址计算算法

### 🆕 源地址计算
```c
// 源为单核数据
src_addr = (src_core_id << 21) | src_offset;

// 源为分布式数据
src_chunk_index = offset / src_chunk_size;
src_target_core = (src_base_core + src_chunk_index) % num_cores;
src_local_offset = offset % src_chunk_size;
src_addr = (src_target_core << 21) | src_local_offset;
```

### 目标地址计算（与之前相同）
```c
// 目标为单核数据
dst_addr = (dst_core_id << 21) | dst_offset;

// 目标为分布式数据
dst_chunk_index = offset / dst_chunk_size;
dst_target_core = (dst_base_core + dst_chunk_index) % num_cores;
dst_local_offset = offset % dst_chunk_size;
dst_addr = (dst_target_core << 21) | dst_local_offset;
```

### 🆕 智能块操作生成
```c
// 为源和目标都为分布式的情况优化
mem_shared_chunk_op_t *ops = mem_shared_generate_chunk_operations(ctx);

for (current_op = ops; current_op; current_op = current_op->next) {
    // 每个块操作包含源和目标的核心信息
    tree src_addr = mem_shared_build_chunk_address(src_base, 
                                                  current_op->source_core,
                                                  current_op->source_offset);
    tree dst_addr = mem_shared_build_chunk_address(dst_base,
                                                  current_op->target_core,
                                                  current_op->target_offset);
    generate_memcpy_call(dst_addr, src_addr, current_op->chunk_size);
}
```

## 性能考虑

### 🆕 性能影响矩阵
| 源类型 | 目标类型 | 调用次数 | 性能影响 | 建议 |
|--------|----------|----------|----------|------|
| 常规内存 | 单核mem_shared | 1 | 无影响 | 最优选择 |
| 单核mem_shared | 常规内存 | 1 | 无影响 | 最优选择 |
| 单核mem_shared | 单核mem_shared | 1 | 无影响 | 最优选择 |
| 常规内存 | 分布mem_shared | N | 轻微影响 | 可以接受 |
| 分布mem_shared | 常规内存 | N | 轻微影响 | 可以接受 |
| 单核mem_shared | 分布mem_shared | 1 | 轻微影响 | 部分填充 |
| 分布mem_shared | 单核mem_shared | 1 | 轻微影响 | 部分复制 |
| 分布mem_shared | 分布mem_shared | N | 明显影响 | 考虑并行优化 |

### 🆕 编译器智能警告
```c
// 触发性能警告的情况
mem_shared char huge_src[100000];    // 分布到8个核心
mem_shared char huge_dst[100000];    // 分布到8个核心
memcpy(huge_dst, huge_src, sizeof(huge_src));

// 编译器警告：
// warning: mem_shared intrinsic operation will generate 8 separate calls 
//          which may impact performance
// note: consider using bulk transfer optimizations for large mem_shared to mem_shared copies
```

### 优化建议
1. **首选单核操作**：单核到单核的操作性能最佳
2. **避免大规模分布到分布**：考虑分阶段复制或专用API
3. **利用部分复制**：只复制需要的数据部分
4. **对齐访问优化**：使用对齐的大小和偏移

## 调试和诊断

### 🆕 增强的编译时信息
使用 `-fdump-mem-shared` 查看详细替换信息：

```bash
gcc -fmem-shared -fmem-shared-cores=4 -fdump-mem-shared program.c
```

🆕 输出示例：
```
[mem_shared] Processing function main for intrinsic replacement
[mem_shared] Replaced memcpy call (mem_shared to regular) with 4 chunk operations
[mem_shared] Generated memcpy source: core 0, offset 0x0000, size 3000
[mem_shared] Generated memcpy source: core 1, offset 0x0000, size 3000
[mem_shared] Generated memcpy source: core 2, offset 0x0000, size 3000
[mem_shared] Generated memcpy source: core 3, offset 0x0000, size 3000

[mem_shared] Replaced memcpy call (mem_shared to mem_shared) with 4 chunk operations
[mem_shared] Generated memcpy target: core 0, offset 0x1000, source: core 0, offset 0x0000, size 3000
[mem_shared] Generated memcpy target: core 1, offset 0x1000, source: core 1, offset 0x0000, size 3000
[mem_shared] Generated memcpy target: core 2, offset 0x1000, source: core 2, offset 0x0000, size 3000
[mem_shared] Generated memcpy target: core 3, offset 0x1000, source: core 3, offset 0x0000, size 3000
```

### 🆕 操作类型识别
编译器会自动识别并标记操作类型：
- `(regular to mem_shared)`: 常规内存到mem_shared
- `(mem_shared to regular)`: mem_shared到常规内存  
- `(mem_shared to mem_shared)`: mem_shared到mem_shared

## 实现细节

### 🆕 增强的上下文结构
```c
typedef struct {
  mem_shared_intrinsic_type_t type;
  tree target_decl;               /* 目标 mem_shared 变量 */
  tree source_decl;               /* 🆕 源 mem_shared 变量 */
  tree size_arg;                  /* 大小参数 */
  tree value_arg;                 /* 值参数（memset用） */
  tree src_arg;                   /* 源参数（复制操作用） */
  mem_shared_chunk_op_t *chunk_ops; /* 块操作列表 */
  unsigned int num_chunks;        /* 块操作数量 */
  bool target_is_distributed;     /* 目标是否分布式 */
  bool source_is_distributed;     /* 🆕 源是否分布式 */
} mem_shared_intrinsic_context_t;
```

### 🆕 增强的块操作描述符
```c
typedef struct mem_shared_chunk_op {
  unsigned int target_core;        /* 目标核心 */
  unsigned int source_core;        /* 🆕 源核心 */
  unsigned int target_offset;      /* 目标核心内偏移 */
  unsigned int source_offset;      /* 🆕 源核心内偏移 */
  unsigned int chunk_size;         /* 块大小 */
  struct mem_shared_chunk_op *next;
} mem_shared_chunk_op_t;
```

## 使用示例

### 🆕 完整的使用场景
```c
#include <string.h>

// 声明各种 mem_shared 变量
mem_shared int small_a[256];         // 单核存储
mem_shared int small_b[256];         // 单核存储
mem_shared double large_a[2000];     // 分布存储
mem_shared double large_b[2000];     // 分布存储

void complete_example() {
    int regular_buffer[2000];
    
    // 场景1：常规 → 单核mem_shared （1个调用）
    memcpy(small_a, regular_buffer, sizeof(small_a));
    
    // 场景2：单核mem_shared → 常规 （1个调用）
    memcpy(regular_buffer, small_a, sizeof(small_a));
    
    // 场景3：单核 → 单核 mem_shared （1个调用）
    memcpy(small_b, small_a, sizeof(small_a));
    
    // 场景4：常规 → 分布mem_shared （4个调用）
    memcpy(large_a, regular_buffer, sizeof(large_a));
    
    // 场景5：分布mem_shared → 常规 （4个调用）
    memcpy(regular_buffer, large_a, sizeof(large_a));
    
    // 场景6：分布 → 分布 mem_shared （4个调用）
    memcpy(large_b, large_a, sizeof(large_a));
    
    // 场景7：部分复制优化
    memcpy(small_a, large_a, sizeof(small_a));  // 只从第一个核心复制
}
```

### 🆕 性能对比示例
```c
void performance_comparison() {
    // 最快：单核对单核
    memcpy(small_b, small_a, sizeof(small_a));           // 1 call
    
    // 快：单核与常规内存
    memcpy(regular_buffer, small_a, sizeof(small_a));    // 1 call
    
    // 中等：单核与分布式（部分操作）
    memcpy(small_a, large_a, sizeof(small_a));           // 1 call
    
    // 较慢：常规与分布式（完整操作）
    memcpy(large_a, regular_buffer, sizeof(large_a));    // 4 calls
    
    // 最慢：分布式对分布式
    memcpy(large_b, large_a, sizeof(large_a));           // 4 calls
}
```

## 最佳实践

### 🆕 针对源操作的建议
1. **优先单核源**：从单核mem_shared读取数据性能最佳
2. **批量读取**：一次性读取大块数据而不是多次小读取
3. **利用局部性**：读取连续的内存区域
4. **避免跨核心随机访问**：这会导致多次分散的内存操作

### 🆕 针对双向操作的建议
1. **考虑数据布局**：源和目标的分布模式应该匹配
2. **使用专用API**：对于频繁的mem_shared间复制，考虑专用函数
3. **分阶段处理**：大数据可以分多个阶段处理
4. **监控性能**：使用编译器警告指导优化

## 局限性和未来发展

### 当前局限性
1. **字符串函数**：字符串操作的源支持仍在开发中
2. **性能开销**：分布到分布的操作有显著开销
3. **运行时优化有限**：大部分优化在编译时进行

### 🆕 未来改进计划
1. **并行传输优化**：mem_shared到mem_shared的并行传输
2. **智能路由**：检测源和目标在相同核心时的零拷贝优化
3. **预取支持**：自动生成预取指令优化性能
4. **DMA集成**：与硬件DMA引擎集成进行大数据传输

这个增强的内置函数支持让 `mem_shared` 变量的使用更加完整和高效，用户现在可以无缝地将这些变量用作任何内置函数的源或目标操作数。