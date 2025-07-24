# mem_shared 部分操作支持

## 🎯 问题背景

在原有实现中，intrinsic函数（如`memcpy`、`memset`）的处理假设操作始终从`mem_shared`变量的起始地址开始，且操作大小等于整个变量的大小。但实际使用中经常出现以下情况：

```c
mem_shared int data[1024];  // 4KB数组
int src[256];               // 普通数组

// 问题场景1：从中间偏移开始操作
memcpy(data + 256, src, 256 * sizeof(int));  // 从第256个元素开始复制

// 问题场景2：操作大小小于变量大小  
memset(data + 512, 0, 128 * sizeof(int));    // 只设置128个元素

// 问题场景3：数组索引表示法
memset(&data[100], 0xFF, 50 * sizeof(int));  // 设置第100-149个元素
```

## ✅ 解决方案概述

### 核心改进
1. **偏移提取**: 自动识别`data + offset`、`&array[index]`等表达式
2. **范围计算**: 精确计算操作的起始位置和大小
3. **智能分块**: 只对受影响的核心生成操作，跳过无关chunks
4. **边界检查**: 编译时验证操作不会越界

### 支持的表达式类型
```c
// 1. 指针算术
mem_shared_var + offset
mem_shared_var + constant_offset

// 2. 数组索引
&mem_shared_array[index]
&mem_shared_array[constant_index]

// 3. 直接变量（传统支持）
mem_shared_var
```

## 🔧 技术实现

### 1. 偏移提取算法

```c
bool mem_shared_extract_pointer_offset(tree pointer_expr, tree *base_decl, 
                                     tree *offset_expr, HOST_WIDE_INT *offset_value) {
    switch (TREE_CODE(pointer_expr)) {
    case POINTER_PLUS_EXPR:
        // 处理 data + offset
        base = TREE_OPERAND(pointer_expr, 0);
        offset = TREE_OPERAND(pointer_expr, 1);
        if (TREE_CODE(offset) == INTEGER_CST)
            *offset_value = tree_to_shwi(offset);
        break;
        
    case ARRAY_REF:
        // 处理 data[index]
        base = TREE_OPERAND(pointer_expr, 0);
        index = TREE_OPERAND(pointer_expr, 1);
        if (TREE_CODE(index) == INTEGER_CST) {
            element_size = int_size_in_bytes(element_type);
            *offset_value = tree_to_shwi(index) * element_size;
        }
        break;
        
    case ADDR_EXPR:
        // 处理 &data[index]
        return extract_offset(TREE_OPERAND(pointer_expr, 0), ...);
    }
}
```

### 2. 智能分块优化

对于分布式数据，只为受影响的核心生成操作：

```c
// 原始操作
memset(large_array + 256, 0, 512 * sizeof(int));  // offset=1024, size=2048

// 分析结果
operation_start = 1024;    // 起始字节偏移
operation_end = 3072;      // 结束字节偏移
chunk_size = 32768 / 108;  // 每核心约303字节

// 只对受影响的核心生成操作
for (core = start_core; core <= end_core; core++) {
    chunk_start = core * chunk_size;
    chunk_end = chunk_start + chunk_size;
    
    if (operation_end <= chunk_start || operation_start >= chunk_end)
        continue;  // 跳过此核心
        
    // 计算重叠区域
    overlap_start = MAX(operation_start, chunk_start);
    overlap_end = MIN(operation_end, chunk_end);
    
    // 生成针对此核心的操作
    generate_chunk_operation(core, overlap_start, overlap_end);
}
```

### 3. 边界检查

```c
bool mem_shared_validate_operation_bounds(ctx) {
    if (ctx->target_offset + ctx->operation_size > target_variable_size) {
        warning("mem_shared operation exceeds target variable bounds: "
               "offset %wd + size %wd > variable size %wd",
               ctx->target_offset, ctx->operation_size, target_variable_size);
        return false;
    }
    return true;
}
```

## 📊 优化效果

### 操作数量减少
```c
// 原实现：总是生成108个操作
mem_shared int big_array[8192];
memset(&big_array[1000], 0, 100 * sizeof(int));

// 旧方式：108个memset调用（浪费）
// 新方式：~3个memset调用（仅受影响的核心）
// 优化率：97.2%
```

### 性能对比

| 操作类型 | 原实现 | 新实现 | 改进 |
|----------|--------|--------|------|
| 小范围操作 | 108个calls | 1-3个calls | 97%+ 减少 |
| 中等范围操作 | 108个calls | 10-20个calls | 81%+ 减少 |
| 大范围操作 | 108个calls | 50-90个calls | 17%+ 减少 |
| 边界检查 | 无 | 编译时检查 | 100% 安全性 |

## 🎯 使用示例

### 1. 基本部分操作

```c
mem_shared int data[1024];
int regular_array[256];

// 场景1：复制到中间位置
memcpy(data + 256, regular_array, 256 * sizeof(int));
// 编译器行为：
// - 检测偏移：256 * 4 = 1024字节
// - 操作大小：256 * 4 = 1024字节
// - 只对包含偏移1024-2048范围的核心生成操作

// 场景2：部分清零
memset(&data[512], 0, 128 * sizeof(int));
// 编译器行为：
// - 检测偏移：512 * 4 = 2048字节
// - 操作大小：128 * 4 = 512字节
// - 只对包含偏移2048-2560范围的核心生成操作
```

### 2. 混合distributed/non-distributed

```c
mem_shared int big_data[2048];     // 8KB - distributed
mem_shared char small_buffer[1024]; // 1KB - single core

// 从分布式拷贝到集中式
memcpy(small_buffer + 256, &big_data[512], 256 * sizeof(int));
// 编译器行为：
// - 源：从多个核心读取big_data[512-767]
// - 目标：写入single core的small_buffer[256-1279]
// - 生成多对一的chunk操作
```

### 3. 边界检查示例

```c
mem_shared int array[100];

// 正确操作
memset(array + 50, 0, 50 * sizeof(int));  // ✅ 通过
memset(&array[0], 0xFF, 100 * sizeof(int)); // ✅ 通过

// 错误操作（编译时警告）
memset(array + 90, 0, 20 * sizeof(int));   // ❌ 越界
// warning: mem_shared operation exceeds target variable bounds:
//          offset 360 + size 80 > variable size 400

memcpy(&array[50], some_data, 60 * sizeof(int)); // ❌ 越界  
// warning: mem_shared operation exceeds target variable bounds:
//          offset 200 + size 240 > variable size 400
```

## 🐛 调试信息

启用`-fdump-mem-shared`可查看详细的操作分析：

```bash
gcc -fmem-shared -fdump-mem-shared partial_ops.c
```

输出示例：
```
[mem_shared] Partial operation detected: target offset 1024 size 1024
[mem_shared] Operation affects cores 8-15 out of 108 total
[mem_shared] Generated 8 chunk operations (skipped 100 unnecessary)
[mem_shared] Chunk 0: group 4 intra 0 offset 0x100 size 256
[mem_shared] Chunk 1: group 4 intra 1 offset 0x100 size 256
[mem_shared] Chunk optimization: reduced operations by 92.6%
```

## 🔬 限制和注意事项

### 当前限制
1. **非常量偏移**: 如果偏移不是编译时常量，回退到运行时处理
2. **复杂指针运算**: 非线性指针计算可能无法优化
3. **动态大小**: 运行时确定的大小参数无法进行编译时优化

### 最佳实践
```c
// ✅ 推荐：编译时常量偏移
#define START_OFFSET 256
memset(data + START_OFFSET, 0, 128 * sizeof(int));

// ⚠️ 可用但不优化：运行时偏移
int runtime_offset = calculate_offset();
memset(data + runtime_offset, 0, size);

// ❌ 避免：复杂指针运算
int *complex_ptr = &data[func1()] + func2();
memset(complex_ptr, 0, size);
```

## 🚀 性能建议

1. **使用常量偏移**: 尽量使用编译时可确定的偏移量
2. **合理操作大小**: 避免过小的频繁操作
3. **对齐访问**: 保持数据对齐以提高访问效率
4. **批量操作**: 合并多个小操作为单个大操作

这个部分操作支持大大提升了`mem_shared`系统的实用性和性能，使其能够处理真实世界中的复杂内存操作场景。