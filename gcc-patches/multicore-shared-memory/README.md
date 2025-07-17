# GCC mem_shared: 多核共享内存编译器扩展

本项目为 GCC 编译器实现了 `mem_shared` 关键字，用于支持众核架构下的共享内存编程。

## 概述

`mem_shared` 关键字允许开发者声明共享内存变量，编译器会自动：
- 根据数据大小和类型选择存储策略
- 为基础类型数据分配到特定从核
- 将大数据自动分布到多个从核
- 生成带核心编号的 ld/st 指令

## 硬件架构支持

- **目标架构**: N 个从核的众核系统
- **内存模型**: 每个从核独立的 512KB local_memory
- **访存指令**: ld/st 指令第21位指定目标从核号
- **跨核访问**: 从核 i 可通过设置第21位访问从核 j 的内存

## 功能特性

### 1. 智能内存分配
- **基础类型** (int, short, float, double): 编译器决定存储从核
- **小数据** (<10KB): 编译器选择最佳适配从核
- **大数据** (≥10KB): 自动均匀分布到所有从核

### 2. 自动代码生成
- 生成带核心号的内存访问指令
- 自动计算分布式数据的目标从核
- 优化单核数据的访问模式

### 3. 编译时优化
- 内存分配冲突检测
- 核心使用率平衡
- 访问模式优化

## 安装和使用

### 编译器构建

1. 应用补丁：
```bash
# 应用所有补丁文件
patch -p1 < c-common.h.patch
patch -p1 < c-common.c.patch  
patch -p1 < c-parser.c.patch
patch -p1 < c-tree.h.patch
patch -p1 < tree.h.patch
patch -p1 < common.opt.patch

# 复制新文件
cp mem-shared.h gcc/
cp mem-shared.c gcc/
```

2. 重新配置和构建：
```bash
cd gcc-build
../configure --enable-mem-shared
make -j$(nproc)
make install
```

### 编译选项

- `-fmem-shared`: 启用 mem_shared 关键字支持
- `-fmem-shared-cores=N`: 指定从核数量 (默认: 1)
- `-fdump-mem-shared`: 输出内存分配信息

### 使用示例

```c
#include <stdio.h>

// 基础类型共享变量
mem_shared int counter = 0;
mem_shared float coefficient = 3.14f;

// 小数组 - 单核存储
mem_shared int buffer[1024];  // 4KB

// 大数组 - 分布式存储  
mem_shared double matrix[1000][1000];  // 8MB

int main() {
    // 编译器自动生成带核心号的指令
    counter++;
    coefficient *= 2.0f;
    
    // 数组访问
    for (int i = 0; i < 1024; i++) {
        buffer[i] = i;
    }
    
    // 分布式矩阵访问
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 1000; j++) {
            matrix[i][j] = i * j;
        }
    }
    
    return 0;
}
```

编译命令：
```bash
gcc -fmem-shared -fmem-shared-cores=4 -fdump-mem-shared example.c -o example
```

## 实现细节

### 编译器前端扩展

1. **词法分析**: 添加 `RID_MEM_SHARED` 关键字
2. **语法分析**: 处理 mem_shared 声明修饰符
3. **语义分析**: 标记和验证 mem_shared 变量

### 内存管理

1. **分配策略**:
   ```c
   typedef enum {
     MEM_SHARED_BASIC_TYPE,    // 基础类型
     MEM_SHARED_SMALL_DATA,    // < 10KB
     MEM_SHARED_LARGE_DATA     // >= 10KB  
   } mem_shared_category_t;
   ```

2. **核心选择算法**:
   - 基础类型: 轮询分配
   - 小数据: 最佳适配算法
   - 大数据: 均匀分布算法

### 代码生成

1. **地址生成**:
   ```c
   // 生成 (core_id << 21) | offset 格式地址
   rtx addr = gen_rtx_IOR(Pmode, 
                         gen_rtx_ASHIFT(Pmode, core_id, GEN_INT(21)),
                         base_addr);
   ```

2. **指令模式**:
   ```lisp
   ;; 共享内存加载指令
   (define_insn "mem_shared_load<mode>"
     [(set (match_operand:GPR 0 "register_operand" "=r")
           (mem:GPR (match_operand:P 1 "mem_shared_address_operand" "")))]
     "TARGET_MULTICORE"
     "ld.<mode>\t%0, %1")
   ```

## 内存布局示例

对于 4 核系统：

```
Core 0 (512KB):
├── global_counter (4B)
├── small_buffer (4KB)  
└── large_matrix[0-249][*] (2MB)

Core 1 (512KB):
├── shared_coefficient (4B)
├── message_buffer (4KB)
└── large_matrix[250-499][*] (2MB)

Core 2 (512KB):
├── precision_value (8B)
├── data_points (24KB)
└── large_matrix[500-749][*] (2MB)

Core 3 (512KB):
├── processing_buffer (4MB chunk)
└── large_matrix[750-999][*] (2MB)
```

## 调试和诊断

### 内存分配信息
使用 `-fdump-mem-shared` 查看分配详情：

```
=== mem_shared Allocation Summary ===
Number of cores: 4
Variable: global_counter
  Category: basic
  Size: 4 bytes
  Distributed: no
  Core: 0
  Offset: 0x0

Variable: large_matrix
  Category: large
  Size: 8000000 bytes
  Distributed: yes
  Chunks: 4
  Chunk size: 2000000 bytes
```

### 核心使用率
```
=== Core Memory Usage ===
Core 0:
  Used: 2048 bytes (0.4%)
  Available: 522240 bytes
  Allocations: 2

Core 1:
  Used: 4100 bytes (0.8%)
  Available: 520188 bytes  
  Allocations: 2
```

## 错误处理

编译器会检测和报告：

1. **内存溢出**: 
   ```
   error: core 0 memory overflow: need 600000 bytes, only 524288 available
   ```

2. **无效声明**:
   ```
   error: invalid mem_shared declaration 'func': mem_shared cannot be applied to functions
   ```

3. **大小计算错误**:
   ```
   error: cannot determine size of mem_shared variable 'var'
   ```

## 性能考虑

### 优化建议

1. **数据布局**: 相关数据尽量声明在一起
2. **访问模式**: 顺序访问比随机访问效率更高  
3. **核心平衡**: 避免所有大数据集中在少数核心

### 性能指标

- **基础类型访问**: 单指令延迟
- **单核数据访问**: 本地内存速度
- **分布式数据访问**: 跨核通信延迟

## 限制和注意事项

1. **同步**: 编译器不提供自动加锁，需用户实现
2. **一致性**: 不保证跨核内存一致性
3. **原子性**: 不支持原子操作，需额外同步机制
4. **函数指针**: mem_shared 不能修饰函数
5. **动态内存**: 仅支持静态分配的变量

## 扩展和定制

### 目标特定扩展

可以重写目标特定函数：

```c
// 自定义地址编码
rtx mem_shared_target_encode_address(unsigned int core, unsigned int offset)
{
  // 目标特定的地址编码实现
}

// 自定义核心数量
unsigned int mem_shared_target_get_core_count(void)
{
  // 返回目标平台的核心数
}
```

### 优化插件

可以编写 GCC 插件进一步优化：
- 访问模式分析
- 数据重排优化  
- 预取指令插入

## 测试和验证

### 单元测试
```bash
cd gcc/testsuite
make check-gcc RUNTESTFLAGS="mem-shared.exp"
```

### 集成测试
```bash
# 编译示例程序
gcc -fmem-shared -fmem-shared-cores=4 examples/*.c

# 运行功能测试
./run-tests.sh
```

## 参与贡献

1. Fork 项目
2. 创建功能分支
3. 提交更改
4. 创建 Pull Request

详细的贡献指南请参考 [CONTRIBUTING.md](CONTRIBUTING.md)

## 许可证

本项目遵循 GPL v3 许可证，详见 [LICENSE](LICENSE) 文件。

## 联系方式

- **问题报告**: [GitHub Issues](https://github.com/gcc/gcc/issues)
- **邮件列表**: gcc@gcc.gnu.org
- **文档**: [GCC官方文档](https://gcc.gnu.org/onlinedocs/)

## 更新日志

### v1.0.0 (2024-01-XX)
- 初始实现 mem_shared 关键字
- 支持基础类型、小数据、大数据的自动分配
- 实现分布式存储算法
- 添加编译选项和调试支持

### v1.1.0 (计划中)
- 优化内存分配算法
- 添加访问模式分析
- 支持动态核心数配置
- 改进错误诊断信息