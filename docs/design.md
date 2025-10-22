# 众核共享内存 GCC 编译器实现方案

## 概述

本方案为 GCC 编译器设计了一个新的关键字 `mem_shared`，用于在众核架构下实现共享内存功能。系统包含 N 个从核，每个从核有独立的 512KB local_memory，通过 ld/st 指令的第21位指定从核号来实现跨核访存。

## 系统架构

### 硬件架构
- N 个从核（从核编号：0 到 N-1）
- 每个从核独立的 local_memory：512KB
- ld/st 指令格式：第21位指定目标从核号
- 支持从核 i 通过 ld/st 指令访问从核 j 的 local_memory

### 内存布局策略
1. **基础类型数据**（int, short, float, double等）：由编译器决定存储在特定从核
2. **小于 10KB 的数据**：由编译器决定存储位置
3. **大于等于 10KB 的数据**：均匀分布到不同从核

## 实现方案

### 1. 关键字定义和语法

#### 1.1 新增关键字
```c
// 新增 mem_shared 关键字
mem_shared int shared_var;
mem_shared float shared_array[1000];
mem_shared struct data_struct shared_data;
```

#### 1.2 语法规则
- `mem_shared` 只能修饰变量声明
- 支持所有基础数据类型和用户定义类型
- 支持数组和结构体
- 不支持修饰函数和类型定义

### 2. GCC 编译器扩展

#### 2.1 词法分析扩展 (c-family/c-common.h)
```c
// 在 c_common_reswords 中添加新关键字
{ "mem_shared", RID_MEM_SHARED, D_CONLY },
```

#### 2.2 语法分析扩展 (c/c-parser.c)
```c
// 在 c_parser_declspecs 中处理 mem_shared
case RID_MEM_SHARED:
  dspec = build_attr_spec(C_LOC_BEGIN_PRAGMA, NULL_TREE,
                         build_tree_list(get_identifier("mem_shared"), NULL_TREE));
  declspecs_add_attrs(loc, specs, dspec);
  c_parser_consume_token(parser);
  break;
```

#### 2.3 属性系统扩展 (gcc/tree.h)
```c
// 添加新的属性标识
#define DECL_MEM_SHARED_P(NODE) \
  (DECL_WITH_VIS_CHECK (NODE)->decl_with_vis.mem_shared_flag)

// 在 decl_with_vis 结构中添加标志位
struct GTY(()) tree_decl_with_vis {
  // ... 现有字段 ...
  unsigned mem_shared_flag : 1;
  // ... 其他字段 ...
};
```

### 3. 内存分配策略

#### 3.1 数据分类器
```c
// 数据大小分类
typedef enum {
  MEM_SHARED_BASIC_TYPE,    // 基础类型
  MEM_SHARED_SMALL_DATA,    // < 10KB
  MEM_SHARED_LARGE_DATA     // >= 10KB
} mem_shared_category_t;

// 内存分配信息
typedef struct {
  mem_shared_category_t category;
  unsigned int target_core;     // 目标从核号
  unsigned int start_offset;    // 起始偏移
  unsigned int size;           // 数据大小
  bool is_distributed;         // 是否分布式存储
} mem_shared_info_t;
```

#### 3.2 分配算法
```c
// 核心分配函数
static mem_shared_info_t*
allocate_mem_shared_space(tree decl)
{
  mem_shared_info_t *info = XNEW(mem_shared_info_t);
  unsigned int data_size = tree_to_uhwi(TYPE_SIZE_UNIT(TREE_TYPE(decl)));
  
  info->size = data_size;
  
  if (is_basic_type(TREE_TYPE(decl))) {
    info->category = MEM_SHARED_BASIC_TYPE;
    info->target_core = get_next_available_core();
    info->is_distributed = false;
  }
  else if (data_size < 10240) { // 10KB
    info->category = MEM_SHARED_SMALL_DATA;
    info->target_core = get_best_fit_core(data_size);
    info->is_distributed = false;
  }
  else {
    info->category = MEM_SHARED_LARGE_DATA;
    info->is_distributed = true;
    distribute_across_cores(info, data_size);
  }
  
  return info;
}
```

### 4. 代码生成

#### 4.1 地址生成
```c
// 生成共享内存访问地址
rtx
generate_shared_mem_address(tree decl, HOST_WIDE_INT offset)
{
  mem_shared_info_t *info = get_mem_shared_info(decl);
  
  if (!info->is_distributed) {
    // 单核存储：生成带核心号的地址
    rtx core_id = GEN_INT(info->target_core);
    rtx base_addr = GEN_INT(info->start_offset + offset);
    
    // 设置第21位为核心号
    rtx addr = gen_rtx_IOR(Pmode, 
                          gen_rtx_ASHIFT(Pmode, core_id, GEN_INT(21)),
                          base_addr);
    return addr;
  }
  else {
    // 分布式存储：计算目标核心和偏移
    unsigned int chunk_size = info->size / num_cores;
    unsigned int target_core = offset / chunk_size;
    unsigned int local_offset = offset % chunk_size;
    
    rtx core_id = GEN_INT((info->target_core + target_core) % num_cores);
    rtx base_addr = GEN_INT(info->start_offset + local_offset);
    
    rtx addr = gen_rtx_IOR(Pmode,
                          gen_rtx_ASHIFT(Pmode, core_id, GEN_INT(21)),
                          base_addr);
    return addr;
  }
}
```

#### 4.2 Load/Store 指令生成
```c
// 生成共享内存读取指令
rtx
expand_mem_shared_load(tree decl, HOST_WIDE_INT offset, machine_mode mode)
{
  rtx addr = generate_shared_mem_address(decl, offset);
  rtx mem = gen_rtx_MEM(mode, addr);
  
  // 设置内存属性
  set_mem_alias_set(mem, get_mem_shared_alias_set());
  MEM_VOLATILE_P(mem) = 1; // 防止优化
  
  return mem;
}

// 生成共享内存写入指令
void
expand_mem_shared_store(tree decl, rtx value, HOST_WIDE_INT offset)
{
  machine_mode mode = TYPE_MODE(TREE_TYPE(decl));
  rtx addr = generate_shared_mem_address(decl, offset);
  rtx mem = gen_rtx_MEM(mode, addr);
  
  set_mem_alias_set(mem, get_mem_shared_alias_set());
  MEM_VOLATILE_P(mem) = 1;
  
  emit_move_insn(mem, value);
}
```

### 5. 目标机器描述扩展

#### 5.1 新增指令模式 (target.md)
```lisp
;; 共享内存加载指令
(define_insn "mem_shared_load<mode>"
  [(set (match_operand:GPR 0 "register_operand" "=r")
        (mem:GPR (match_operand:P 1 "mem_shared_address_operand" "")))]
  "TARGET_MULTICORE"
  "ld.<mode>\t%0, %1"
  [(set_attr "type" "load")
   (set_attr "length" "4")])

;; 共享内存存储指令  
(define_insn "mem_shared_store<mode>"
  [(set (mem:GPR (match_operand:P 0 "mem_shared_address_operand" ""))
        (match_operand:GPR 1 "register_operand" "r"))]
  "TARGET_MULTICORE" 
  "st.<mode>\t%1, %0"
  [(set_attr "type" "store")
   (set_attr "length" "4")])
```

#### 5.2 地址谓词定义
```c
// 共享内存地址谓词
bool
mem_shared_address_operand(rtx op, machine_mode mode)
{
  if (GET_CODE(op) != IOR)
    return false;
    
  rtx left = XEXP(op, 0);
  rtx right = XEXP(op, 1);
  
  // 检查是否为 (core_id << 21) | offset 格式
  if (GET_CODE(left) == ASHIFT) {
    rtx shift_amount = XEXP(left, 1);
    if (CONST_INT_P(shift_amount) && INTVAL(shift_amount) == 21) {
      rtx core_id = XEXP(left, 0);
      return CONST_INT_P(core_id) && CONST_INT_P(right);
    }
  }
  
  return false;
}
```

## 总结

本实现方案提供了完整的 `mem_shared` 关键字支持，包括：

1. **语法扩展**：在 GCC 前端添加新关键字支持
2. **智能分配**：根据数据大小和类型选择存储策略
3. **代码生成**：自动生成带核心号的内存访问指令
4. **调试支持**：提供详细的内存分配信息
5. **编译选项**：灵活的配置选项

该方案实现了编译期自动内存分配，运行时高效访问，为众核架构提供了透明的共享内存编程模型。