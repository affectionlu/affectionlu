# GCC mem_shared: 多核共享内存编译器扩展

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Language: C](https://img.shields.io/badge/Language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Compiler: GCC](https://img.shields.io/badge/Compiler-GCC-green.svg)](https://gcc.gnu.org/)

本项目为 GCC 编译器实现了 `mem_shared` 关键字，用于支持众核架构下的共享内存编程。

## 🎯 项目概述

`mem_shared` 关键字允许开发者声明共享内存变量，编译器会自动：
- 根据数据大小和类型选择存储策略
- 为基础类型数据分配到特定从核
- 将大数据自动分布到多个从核
- 生成带核心编号的 ld/st 指令

## 🏗️ 硬件架构支持

- **目标架构**: N 个从核的众核系统
- **内存模型**: 每个从核独立的 512KB local_memory
- **访存指令**: ld/st 指令第21位指定目标从核号
- **跨核访问**: 从核 i 可通过设置第21位访问从核 j 的内存

## ✨ 功能特性

### 🧠 智能内存分配
- **基础类型** (int, short, float, double): 编译器决定存储从核
- **小数据** (<10KB): 编译器选择最佳适配从核
- **大数据** (≥10KB): 自动均匀分布到所有从核

### ⚡ 自动代码生成
- 生成带核心号的内存访问指令
- 自动计算分布式数据的目标从核
- 优化单核数据的访问模式

### 🔧 编译时优化
- 内存分配冲突检测
- 核心使用率平衡
- 访问模式优化

## 📁 仓库结构

```
gcc-mem-shared/
├── README.md                           # 本文件
├── LICENSE                             # GPL v3 许可证
├── docs/                              # 文档目录
│   ├── design.md                      # 详细设计文档
│   ├── call-flow.md                   # 函数调用流程说明
│   ├── installation.md                # 安装指南
│   └── api-reference.md               # API参考
├── src/                               # 源代码目录
│   ├── mem-shared.h                   # 核心头文件
│   ├── mem-shared.c                   # 核心实现
│   └── mem-shared-updated.c           # 优化版实现
├── patches/                           # GCC补丁文件
│   ├── c-common.h.patch               # 关键字注册
│   ├── c-common.c.patch               # 属性处理
│   ├── c-parser.c.patch               # 语法解析
│   ├── c-tree.h.patch                 # C语言树扩展
│   ├── tree.h.patch                   # 声明标志扩展
│   ├── common.opt.patch               # 编译选项
│   ├── c-decl-patch.diff              # 声明处理
│   ├── expr-patch.diff                # RTL展开
│   └── target.md.patch                # 机器描述
├── examples/                          # 示例代码
│   ├── basic-usage.c                  # 基础使用示例
│   ├── advanced-example.c             # 高级特性示例
│   └── benchmark/                     # 性能测试
├── tests/                             # 测试套件
│   ├── unit/                          # 单元测试
│   ├── integration/                   # 集成测试
│   └── regression/                    # 回归测试
├── scripts/                           # 构建和工具脚本
│   ├── build.sh                       # 构建脚本
│   ├── install.sh                     # 安装脚本
│   └── test.sh                        # 测试脚本
└── contrib/                           # 贡献指南和工具
    ├── CONTRIBUTING.md                # 贡献指南
    ├── coding-style.md                # 代码风格
    └── debug-tools/                   # 调试工具
```

## 🚀 快速开始

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install build-essential git flex bison texinfo

# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install git flex bison texinfo
```

### 构建安装

```bash
# 克隆仓库
git clone https://github.com/your-org/gcc-mem-shared.git
cd gcc-mem-shared

# 应用补丁并构建
./scripts/build.sh

# 安装
sudo ./scripts/install.sh
```

### 基础使用

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
    
    return 0;
}
```

编译命令：
```bash
gcc -fmem-shared -fmem-shared-cores=4 -fdump-mem-shared example.c -o example
```

## 📖 文档

- [📋 详细设计文档](docs/design.md) - 完整的技术设计方案
- [🔄 调用流程说明](docs/call-flow.md) - 函数调用时机和流程
- [⚙️ 安装指南](docs/installation.md) - 详细的构建安装步骤
- [📚 API参考](docs/api-reference.md) - 完整的API文档

## 🧪 测试

```bash
# 运行所有测试
./scripts/test.sh

# 运行特定测试
./scripts/test.sh unit           # 单元测试
./scripts/test.sh integration    # 集成测试
./scripts/test.sh regression     # 回归测试
```

## 📊 性能特性

- **编译时优化**: 零运行时开销的地址计算
- **智能分配**: 三层分配策略自动优化内存布局
- **并行访问**: 大数据自动分布支持并行处理
- **负载均衡**: 动态选择最佳从核分配

## 🔧 编译选项

- `-fmem-shared`: 启用 mem_shared 关键字支持
- `-fmem-shared-cores=N`: 指定从核数量 (默认: 1)
- `-fdump-mem-shared`: 输出内存分配信息

## 🐛 调试

### 内存分配信息
```bash
gcc -fdump-mem-shared -fmem-shared-cores=4 program.c
```

### RTL调试
```bash
gcc -fdump-rtl-expand -fdump-rtl-final -fmem-shared program.c
```

## ⚠️ 限制和注意事项

1. **同步**: 编译器不提供自动加锁，需用户实现
2. **一致性**: 不保证跨核内存一致性
3. **原子性**: 不支持原子操作，需额外同步机制
4. **函数指针**: mem_shared 不能修饰函数
5. **动态内存**: 仅支持静态分配的变量

## 🤝 贡献

我们欢迎社区贡献！请参阅 [贡献指南](contrib/CONTRIBUTING.md) 了解如何参与项目。

### 贡献流程

1. Fork 项目
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

## 📄 许可证

本项目采用 [GPL v3](LICENSE) 许可证 - 详见 LICENSE 文件。

## 👥 作者

- **主要开发者** - *初始工作* - [YourName](https://github.com/yourname)

查看参与此项目的所有 [贡献者](https://github.com/your-org/gcc-mem-shared/contributors) 列表。

## 🙏 致谢

- GCC开发团队提供的优秀编译器框架
- 开源社区的持续支持和贡献
- 众核架构研究领域的前沿工作

## 📞 联系方式

- **问题报告**: [GitHub Issues](https://github.com/your-org/gcc-mem-shared/issues)
- **邮件列表**: gcc-mem-shared@your-org.com
- **文档**: [项目Wiki](https://github.com/your-org/gcc-mem-shared/wiki)

## 📈 路线图

- [x] 基础 mem_shared 关键字实现
- [x] 三层内存分配策略
- [x] RTL代码生成
- [x] 调试和诊断支持
- [ ] 优化算法改进
- [ ] 动态核心配置
- [ ] LLVM后端支持
- [ ] 性能分析工具

---

**⭐ 如果这个项目对您有帮助，请给我们一个星标！**
