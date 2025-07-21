# mem_shared 内置函数处理机制

## 概述

当用户对 `mem_shared` 变量使用 `memset`、`memcpy` 等内置函数时，编译器会自动检测并替换这些调用，生成适合分布式存储的多个函数调用。这确保了即使数据分布在不同核心上，这些操作也能正确执行。

## 问题描述

### 原始问题
```c
mem_shared int data[1024];  // 假设分布存储
memset(data, 1, 1024);      // 原始调用无法正确处理分布式数据
memcpy(dest, data, 1024);   // 同样的问题
```

### 解决方案
编译器自动将单个内置函数调用替换为多个调用，每个调用针对特定的核心和内存段。

## 支持的内置函数

### 内存操作函数
| 函数 | 支持状态 | 说明 |
|------|----------|------|
| `memset` | ✅ 完全支持 | 自动分块设置内存值 |
| `memcpy` | ✅ 完全支持 | 自动分块复制内存 |
| `memmove` | ✅ 完全支持 | 支持重叠内存区域 |
| `memcmp` | ⚠️ 部分支持 | 生成性能警告 |
| `bzero` | ✅ 完全支持 | 等价于 memset(..., 0, ...) |
| `bcopy` | ✅ 完全支持 | 等价于 memcpy |

### 字符串操作函数
| 函数 | 支持状态 | 说明 |
|------|----------|------|
| `strcpy` | 🔄 开发中 | 需要运行时长度计算 |
| `strncpy` | 🔄 开发中 | 固定长度，更容易处理 |
| `strlen` | 🔄 开发中 | 需要跨核心搜索 |
| `strcmp` | 🔄 开发中 | 需要跨核心比较 |
| `strncmp` | 🔄 开发中 | 固定长度比较 |

## 实现机制

### 1. 检测阶段
编译器在 GIMPLE 优化阶段插入专门的 pass 来检测内置函数调用：

```c
// 检测流程
if (is_gimple_call(stmt)) {
    tree fndecl = gimple_call_fndecl(call);
    if (mem_shared_detect_intrinsic(fndecl) != UNKNOWN) {
        tree target = mem_shared_get_target_from_intrinsic(call);
        if (target && mem_shared_decl_p(target)) {
            // 发现需要处理的调用
            replace_intrinsic_call(call);
        }
    }
}
```

### 2. 分析阶段
确定数据分布情况和操作范围：

```c
mem_shared_intrinsic_context_t ctx;
mem_shared_analyze_intrinsic_call(call_expr, &ctx);

// 分析结果包括：
// - 目标变量的分布情况
// - 操作大小和范围  
// - 需要生成的块操作数量
```

### 3. 代码生成阶段
根据数据分布生成相应的函数调用：

#### 单核数据示例
```c
// 原始代码
mem_shared int small_array[256];  // 1KB，单核存储
memset(small_array, 0, sizeof(small_array));

// 编译器生成
memset((void*)((2 << 21) | 0x800), 0, 1024);
//     地址编码：核心2，偏移0x800
```

#### 分布式数据示例
```c
// 原始代码
mem_shared int large_array[3000];  // 12KB，分布存储
memset(large_array, 42, sizeof(large_array));

// 编译器生成（4核系统）
memset((void*)((0 << 21) | 0x0000), 42, 3000);  // 核心0
memset((void*)((1 << 21) | 0x0000), 42, 3000);  // 核心1  
memset((void*)((2 << 21) | 0x0000), 42, 3000);  // 核心2
memset((void*)((3 << 21) | 0x0000), 42, 3000);  // 核心3
```

## 地址计算算法

### 分布式数据地址计算
```c
// 计算目标核心
unsigned int chunk_index = offset / chunk_size;
unsigned int target_core = (base_core + chunk_index) % num_cores;

// 计算核心内偏移
unsigned int local_offset = offset % chunk_size;

// 生成编码地址
address = (target_core << 21) | local_offset;
```

### 部分操作优化
编译器能够优化只影响部分数据的操作：

```c
// 原始代码
mem_shared int array[4000];  // 16KB，分布式
memset(array, 0, 1000);      // 只设置前1000个int（4KB）

// 编译器分析：只影响第一个核心
memset((void*)((0 << 21) | 0x0000), 0, 4000);  // 只生成一个调用
```

## 性能考虑

### 性能影响分析
| 数据类型 | 调用次数 | 性能影响 | 建议 |
|----------|----------|----------|------|
| 单核数据 | 1 | 无影响 | 正常使用 |
| 小分布数据 | 2-4 | 轻微影响 | 可以接受 |
| 大分布数据 | 4+ | 明显影响 | 考虑批量操作 |

### 编译器警告
编译器会对可能影响性能的操作发出警告：

```c
// 触发警告的情况
mem_shared char huge_buffer[100000];  // 分布到8个核心
memset(huge_buffer, 0, sizeof(huge_buffer));

// 编译器警告：
// warning: mem_shared intrinsic operation will generate 8 separate calls 
//          which may impact performance
```

### 优化建议
1. **批量操作**：尽量减少小块频繁操作
2. **对齐访问**：使用对齐的大小和偏移
3. **局部性原则**：优先操作连续的内存区域

## 调试和诊断

### 编译时信息
使用 `-fdump-mem-shared` 查看替换详情：

```bash
gcc -fmem-shared -fmem-shared-cores=4 -fdump-mem-shared program.c
```

输出示例：
```
[mem_shared] Processing function main for intrinsic replacement
[mem_shared] Replaced memset call with 4 chunk operations
[mem_shared] Generated memset for core 0, offset 0x0000, size 3000
[mem_shared] Generated memset for core 1, offset 0x0000, size 3000
[mem_shared] Generated memset for core 2, offset 0x0000, size 3000
[mem_shared] Generated memset for core 3, offset 0x0000, size 3000
```

### GIMPLE 转储
使用 `-fdump-tree-mem_shared_intrinsics` 查看详细的 AST 变换：

```bash
gcc -fmem-shared -fdump-tree-mem_shared_intrinsics program.c
```

## 实现细节

### GCC Pass 集成
内置函数替换通过专门的 GIMPLE pass 实现：

```c
// pass 定义
const pass_data pass_data_mem_shared_intrinsics = {
  GIMPLE_PASS,                    // pass 类型
  "mem_shared_intrinsics",        // pass 名称  
  OPTGROUP_NONE,                  // 优化组
  TV_NONE,                        // 时间变量
  PROP_cfg | PROP_ssa,           // 需要的属性
  0,                             // 提供的属性
  0,                             // 破坏的属性
  0,                             // 开始时的 todo
  TODO_update_ssa | TODO_cleanup_cfg  // 结束时的 todo
};
```

### 块操作生成
```c
// 生成块操作描述符
typedef struct mem_shared_chunk_op {
  unsigned int target_core;        // 目标核心
  unsigned int local_offset;       // 核心内偏移  
  unsigned int chunk_size;         // 块大小
  void *src_ptr;                  // 源指针（复制操作）
  void *dst_ptr;                  // 目标指针
  struct mem_shared_chunk_op *next;
} mem_shared_chunk_op_t;
```

## 使用示例

### 基础使用
```c
#include <string.h>

// 声明 mem_shared 变量
mem_shared int data[1024];           // 4KB，单核存储
mem_shared double matrix[500][500];  // 2MB，分布存储

void example() {
    // 自动替换的操作
    memset(data, 0, sizeof(data));           // 单个调用
    memset(matrix, 0, sizeof(matrix));       // 多个调用
    
    // 复制操作
    int source[1024];
    memcpy(data, source, sizeof(data));      // 单个调用
}
```

### 高级使用
```c
void advanced_example() {
    mem_shared char buffer[20000];  // 20KB，分布存储
    
    // 部分操作（只影响部分核心）
    memset(buffer, 'A', 5000);      // 可能只需要2个调用
    
    // 动态大小操作
    size_t size = get_dynamic_size();
    memset(buffer, 0, size);        // 运行时计算块数
    
    // 字符串操作（将来支持）
    strcpy(buffer, "Hello world");  // 自动处理字符串长度
}
```

## 局限性和未来发展

### 当前局限性
1. **字符串函数**：部分字符串函数尚未完全实现
2. **性能开销**：分布式操作有多次调用开销
3. **运行时大小**：动态大小计算可能影响优化

### 未来改进
1. **并行优化**：生成并行执行的内置函数调用
2. **向量化**：利用 SIMD 指令优化块操作
3. **智能合并**：合并相邻的小操作
4. **缓存优化**：考虑缓存行对齐的优化

## 最佳实践

### 编程建议
1. **预先声明大小**：尽量使用编译时常量大小
2. **避免频繁小操作**：批量处理数据
3. **注意性能警告**：及时响应编译器警告
4. **测试验证**：验证分布式操作的正确性

### 性能优化
1. **使用性能分析工具**：测量实际性能影响
2. **考虑数据布局**：根据访问模式调整分布策略
3. **利用编译器优化**：启用相关优化选项

这个机制极大地简化了 `mem_shared` 变量的使用，让开发者可以像使用普通变量一样使用内置函数，而编译器会自动处理底层的复杂性。