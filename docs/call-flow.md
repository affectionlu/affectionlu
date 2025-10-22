# mem_shared 函数调用流程详解

## GCC编译阶段概览

```
源代码 → 词法分析 → 语法分析 → 语义分析 → GIMPLE → RTL展开 → 指令选择 → 寄存器分配 → 汇编输出
```

## 详细调用流程

### 1. `mem_shared_process_declaration` 调用时机

**阶段**: 语义分析 (Semantic Analysis)
**时机**: 变量声明完成时
**调用路径**:
```
c-parser.c: c_parser_declaration
    ↓
c-decl.c: finish_decl()
    ↓ 
c-decl.c: mem_shared_process_declaration()  ← 在这里被调用
```

**具体触发条件**:
```c
/* 在 finish_decl() 函数中 */
if (TREE_CODE (decl) == VAR_DECL && 
    lookup_attribute ("mem_shared", DECL_ATTRIBUTES (decl)))
  {
    mem_shared_process_declaration (decl);  // 这里调用
  }
```

**处理过程**:
1. 检测到 `mem_shared` 属性的变量声明
2. 计算变量大小
3. 根据分配策略选择目标从核
4. 在内存分配表中注册
5. 设置 `DECL_MEM_SHARED_P(decl) = 1`

**示例**:
```c
mem_shared int counter;  // 触发 mem_shared_process_declaration(counter_decl)
```

### 2. `mem_shared_expand_load` 调用时机

**阶段**: RTL展开 (RTL Expansion) 
**时机**: 将GIMPLE转换为RTL时遇到mem_shared变量访问
**调用路径**:
```
gimple → RTL expansion
    ↓
expr.c: expand_expr_real_1()
    ↓
case VAR_DECL: 检查 DECL_MEM_SHARED_P(exp)
    ↓
mem_shared_expand_load()  ← 在这里被调用
```

**具体触发条件**:
```c
/* 在 expand_expr_real_1() 函数中 */
if (VAR_P (exp) && DECL_MEM_SHARED_P (exp))
  {
    rtx mem_ref = mem_shared_expand_load (exp, 0, mode);  // 这里调用
    return mem_ref;
  }
```

**处理过程**:
1. 检测到对mem_shared变量的读取访问
2. 调用 `mem_shared_generate_address()` 生成特殊地址
3. 创建带有核心号编码的内存引用RTL
4. 返回可以被指令选择器识别的RTL模式

**示例**:
```c
int x = counter;  // 触发 mem_shared_expand_load(counter_decl, 0, SImode)
```

### 3. `mem_shared_load` insn 调用时机

**阶段**: 指令选择 (Instruction Selection)
**时机**: RTL模式匹配阶段
**调用路径**:
```
RTL expansion 产生的 RTL patterns
    ↓
instruction selection (insn-recog.c)
    ↓
pattern matching against target.md
    ↓
匹配到 mem_shared_load<mode> pattern  ← 在这里被匹配和调用
```

**匹配过程**:
```lisp
;; 当指令选择器看到这种RTL模式时:
(set (reg:SI 10) 
     (mem:SI (ior:SI (ashift:SI (const_int 2) (const_int 21))
                     (const_int 0x1000))))

;; 会匹配到这个指令模式:
(define_insn "*mem_shared_load<mode>"
  [(set (match_operand:GPR 0 "register_operand" "=r")
        (mem:GPR (match_operand:P 1 "mem_shared_address_operand" "")))]
  "TARGET_MULTICORE"
  "ld.<mode>\t%0, %1\t// mem_shared load")
```

**生成的汇编**:
```assembly
ld.w    r10, 0x401000   // 第21位编码了从核号2
```

## 具体示例分析

### 示例代码:
```c
mem_shared int global_var = 42;
mem_shared int buffer[1000];

void test_function() {
    int x = global_var;        // 触发 load
    global_var = 100;          // 触发 store  
    buffer[10] = x + 1;        // 触发 array store
}
```

### 编译过程调用序列:

#### 1. 声明处理阶段:
```
解析 "mem_shared int global_var = 42;"
    ↓
c_parser_declspecs() 识别 mem_shared 关键字
    ↓ 
finish_decl() 调用 mem_shared_process_declaration(global_var)
    ↓
分配: global_var → 从核0, 偏移0x0000
```

#### 2. RTL展开阶段:
```
展开 "int x = global_var;"
    ↓
expand_expr_real_1(global_var, ...)
    ↓
检测到 DECL_MEM_SHARED_P(global_var) == 1
    ↓
调用 mem_shared_expand_load(global_var, 0, SImode)
    ↓
生成 RTL: (set (reg:SI 10) (mem:SI (const_int 0x000000)))
```

#### 3. 指令选择阶段:
```
指令选择器处理 RTL pattern
    ↓
匹配到 mem_shared_load 指令模式
    ↓
生成汇编: "ld.w r10, 0x000000"
```

## 关键函数的职责分工

### `mem_shared_process_declaration()`
- **职责**: 静态内存分配和布局规划
- **输入**: 变量声明树节点
- **输出**: 内存分配信息，更新全局分配表
- **调用频率**: 每个mem_shared变量声明一次

### `mem_shared_expand_load()`
- **职责**: 生成RTL内存访问模式
- **输入**: 变量、偏移、访问模式
- **输出**: RTL内存引用
- **调用频率**: 每次读取mem_shared变量时

### `mem_shared_load` 指令模式
- **职责**: 最终代码生成
- **输入**: RTL模式
- **输出**: 目标汇编指令
- **调用频率**: 指令选择时，当RTL模式匹配成功

## 时序图

```
时间轴: 编译开始 ────────────────────────────────────────── 编译结束
         │         │              │                 │
      声明解析   语义分析      RTL展开           指令选择
         │         │              │                 │
         │    process_decl()   expand_load()   insn匹配
         │         │              │                 │
      关键字识别  内存分配    生成RTL模式      生成汇编指令
```

## 调试方法

### 1. 调试声明处理:
```bash
gcc -fdump-mem-shared -fmem-shared-cores=4 test.c
```

### 2. 调试RTL展开:
```bash  
gcc -fdump-rtl-expand -fmem-shared test.c
```

### 3. 调试指令选择:
```bash
gcc -fdump-rtl-final -fmem-shared test.c
```

这样可以清楚地看到每个阶段的处理结果和函数调用情况。