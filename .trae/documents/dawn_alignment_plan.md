# Dawn 项目完整对齐计划

## 概述

本计划将利用多 Agent 并行工作，依次完成 `DAWN_FLUENT_ALIGNMENT.md` 中所有未对齐的部分，确保 Dawn 项目完全可用。

## 当前状态分析

根据 `DAWN_FLUENT_ALIGNMENT.md`：

### 已完成的组件
- 基础架构 (MVVM + Service + Pipeline)
- FluentUI 集成
- 实例管理核心
- 内容提供者接口
- 任务管线
- 预检服务
- 下载服务
- 设置服务
- 内容安装服务
- UI 页面框架

### 部分实现的组件
- 账户系统 (有接口，需完整实现 OAuth)
- Java 管理 (有服务，需实现自动发现/下载)
- Loader 安装 (有接口，需实现各 Loader 安装器)
- 备份服务 (有接口，需实现快照/回滚)
- 诊断服务 (有框架，需实现日志解释)

### 待实现的组件 (P0 - 最高优先级)
- Minecraft 版本下载
- 库文件下载与校验
- 启动命令构建
- 实例创建向导
- 拖拽安装
- 智能性能建议

## 实施阶段

### Phase 1: P0 核心功能 (立即执行)

#### 任务 1.1: Minecraft 版本服务完善
**负责人**: cpp-expert Agent
**依赖**: 无
**目标**:
- 实现版本清单下载 (launcher metadata) - 已有基础，需完善
- 实现版本 JSON 解析
- 实现库文件下载和校验

**需要创建的文件**:
- `core/include/dawn/core/minecraft/version_package.h` - 版本包模型
- `core/include/dawn/core/minecraft/library_resolver.h` - 库文件解析器
- `core/src/minecraft/version_package.cpp`
- `core/src/minecraft/library_resolver.cpp`

**需要修改的文件**:
- `core/src/minecraft/minecraft_service.cpp` - 添加版本下载逻辑

#### 任务 1.2: 启动运行时完善
**负责人**: cpp-expert Agent
**依赖**: 任务 1.1
**目标**:
- 完善 `DefaultLauncherRuntime`
- 构建完整的启动命令 (包括类路径、JVM 参数等)
- 实现启动前预检

**需要修改的文件**:
- `core/src/interfaces/default_launcher_runtime.cpp`

#### 任务 1.3: Loader 安装器实现
**负责人**: cpp-expert Agent
**依赖**: 无
**目标**:
- 实现 Fabric 安装器
- 实现 Forge 安装器
- 实现 NeoForge 安装器
- 实现 Quilt 安装器

**需要创建的文件**:
- `core/include/dawn/core/loaders/fabric_installer.h`
- `core/include/dawn/core/loaders/forge_installer.h`
- `core/include/dawn/core/loaders/neoforge_installer.h`
- `core/include/dawn/core/loaders/quilt_installer.h`
- `core/src/loaders/fabric_installer.cpp`
- `core/src/loaders/forge_installer.cpp`
- `core/src/loaders/neoforge_installer.cpp`
- `core/src/loaders/quilt_installer.cpp`

**需要修改的文件**:
- `core/src/loaders/loader_service.cpp` - 集成各 Loader 安装器

### Phase 2: P1 增强功能

#### 任务 2.1: 账户系统完善
**负责人**: cpp-expert Agent
**依赖**: 无
**目标**:
- 实现 Microsoft OAuth 设备流
- 实现离线档案
- 实现多账户切换
- 实现账户失效检测与刷新

**需要创建的文件**:
- `core/include/dawn/core/auth/offline_account_service.h`
- `core/src/auth/offline_account_service.cpp`

**需要修改的文件**:
- `core/src/auth/account_service.cpp` - 完善账户管理
- `core/src/auth/microsoft_oauth_service.cpp` - 实现 OAuth 流程

#### 任务 2.2: Java 管理完善
**负责人**: cpp-expert Agent
**依赖**: 无
**目标**:
- 实现 Java 自动发现 (扫描系统路径)
- 实现 Java 下载 (从 Adoptium 等源)
- 实现 Java 版本匹配逻辑

**需要修改的文件**:
- `core/src/java/java_service.cpp`

#### 任务 2.3: 实例创建向导
**负责人**: qt-specialist Agent
**依赖**: 任务 1.1, 1.3, 2.2
**目标**:
- 实现 4 步向导 UI
- 集成版本和 Loader 选择
- 集成 Java 配置

**需要创建的文件**:
- `ui/qml/dialogs/InstanceCreationWizard.qml`
- `ui/qml/components/WizardStepIndicator.qml`
- `ui/qml/components/VersionSelector.qml`
- `ui/qml/components/LoaderSelector.qml`

**需要修改的文件**:
- `ui/qml/pages/InstancesPage.qml` - 添加创建实例入口

### Phase 3: P2 高级功能

#### 任务 3.1: 备份系统
**负责人**: cpp-expert Agent
**依赖**: 无
**目标**:
- 实现快照功能
- 实现回滚功能
- 实现自动备份策略

**需要修改的文件**:
- `core/src/backup/backup_service.cpp`

#### 任务 3.2: 诊断服务
**负责人**: cpp-expert Agent
**依赖**: 无
**目标**:
- 实现日志解释
- 实现人话总结
- 实现常见问题检测

**需要修改的文件**:
- `core/src/diagnostics/diagnostics_service.cpp`

#### 任务 3.3: 拖拽安装
**负责人**: qt-specialist Agent
**依赖**: 无
**目标**:
- 实现文件拖拽检测
- 实现自动分类 (mod/resourcepack/shader/modpack)
- 实现自动安装

**需要创建的文件**:
- `ui/qml/components/DropZone.qml`
- `ui/qml/dialogs/DragDropInstallDialog.qml`

### Phase 4: P3 优化体验

#### 任务 4.1: 智能建议
**负责人**: cpp-expert Agent
**依赖**: 任务 1.1, 1.2, 2.2
**目标**:
- 实现性能建议 (根据实例类型)
- 实现更新模拟 (显示影响分析)

**需要创建的文件**:
- `core/include/dawn/core/diagnostics/performance_advisor.h`
- `core/src/diagnostics/performance_advisor.cpp`

#### 任务 4.2: 动效优化
**负责人**: qt-specialist Agent
**依赖**: 无
**目标**:
- 完善页面过渡动画
- 优化交互反馈
- 实现 Fluent Design 动效

**需要修改的文件**:
- `ui/qml/shell/AppShell.qml`
- `ui/qml/pages/*.qml` - 各页面添加动效

## 多 Agent 协作策略

### Agent 分工

1. **cpp-expert Agent**: 负责所有 C++ 核心服务实现
   - Minecraft 版本服务
   - 启动运行时
   - Loader 安装器
   - 账户系统
   - Java 管理
   - 备份系统
   - 诊断服务
   - 智能建议

2. **qt-specialist Agent**: 负责所有 QML UI 实现
   - 实例创建向导
   - 拖拽安装
   - 动效优化
   - 页面过渡动画

### 执行顺序

```
Phase 1 (并行):
  - 任务 1.1 (cpp-expert)
  - 任务 1.3 (cpp-expert)

Phase 2 (任务 1.1 和 1.3 完成后):
  - 任务 1.2 (cpp-expert)
  - 任务 2.1 (cpp-expert)
  - 任务 2.2 (cpp-expert)
  - 任务 2.3 (qt-specialist)

Phase 3 (并行):
  - 任务 3.1 (cpp-expert)
  - 任务 3.2 (cpp-expert)
  - 任务 3.3 (qt-specialist)

Phase 4 (并行):
  - 任务 4.1 (cpp-expert)
  - 任务 4.2 (qt-specialist)
```

## 技术规范

### 代码规范
- 使用 C++20 标准
- 遵循现有命名约定
- 所有 public 接口必须有文档注释
- 使用 `std::optional` 处理可能失败的返回值

### 错误处理
- 使用 `std::string* error` 参数传递错误信息
- 所有网络操作必须有超时和重试机制
- 文件操作必须处理 `std::error_code`

### 测试要求
- 所有新组件必须有单元测试
- 集成测试覆盖主要用户流程
- 使用 Google Test 框架

## 验收标准

### P0 验收标准
- [ ] 可以下载任意 Minecraft 版本
- [ ] 可以安装 Fabric/Forge/NeoForge
- [ ] 可以构建有效的启动命令
- [ ] 启动前预检可以检测常见问题

### P1 验收标准
- [ ] 可以登录 Microsoft 账户
- [ ] 可以创建离线档案
- [ ] 可以自动发现和下载 Java
- [ ] 实例创建向导可以完整运行

### P2 验收标准
- [ ] 可以创建实例快照
- [ ] 可以回滚到之前的状态
- [ ] 日志可以显示人话解释
- [ ] 可以拖拽安装文件

### P3 验收标准
- [ ] 根据实例类型给出性能建议
- [ ] 更新前显示影响分析
- [ ] 所有页面切换有流畅动画

## 实施检查清单

### Phase 1
- [ ] 任务 1.1: Minecraft 版本服务完善
- [ ] 任务 1.2: 启动运行时完善
- [ ] 任务 1.3: Loader 安装器实现

### Phase 2
- [ ] 任务 2.1: 账户系统完善
- [ ] 任务 2.2: Java 管理完善
- [ ] 任务 2.3: 实例创建向导

### Phase 3
- [ ] 任务 3.1: 备份系统
- [ ] 任务 3.2: 诊断服务
- [ ] 任务 3.3: 拖拽安装

### Phase 4
- [ ] 任务 4.1: 智能建议
- [ ] 任务 4.2: 动效优化

## 风险与缓解

### 风险 1: Loader 安装器复杂度高
**缓解**: 参考现有开源实现 (HMCL, Prism Launcher)，优先实现 Fabric (最简单)，再逐步完成其他 Loader。

### 风险 2: Microsoft OAuth 流程复杂
**缓解**: 使用设备流 (Device Flow) 简化实现，避免处理浏览器回调。

### 风险 3: 版本库文件解析复杂
**缓解**: 使用 Minecraft 官方 launcher metadata，参考官方启动器实现。

### 风险 4: 多 Agent 协作冲突
**缓解**: 严格按 Phase 执行，每个 Phase 内并行但不同文件，避免同时修改同一文件。
