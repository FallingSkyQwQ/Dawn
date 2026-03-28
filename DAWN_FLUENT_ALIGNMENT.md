# Dawn 与 dawn.md 对齐方案

## 执行摘要

本文档记录 Dawn 项目当前状态与 `dawn.md` 产品规范的对比分析，并提供完整的实现路线图。

---

## 一、现状评估

### 1.1 已实现的组件

| 组件 | 状态 | 说明 |
|------|------|------|
| 基础架构 (MVVM + Service + Pipeline) | ✅ 已完成 | 核心架构已落地 |
| FluentUI 集成 | ✅ 已完成 | Qt6 + FluentUI 已集成 |
| 实例管理核心 | ✅ 已完成 | InstanceService, InstanceManifest |
| 内容提供者接口 | ✅ 已完成 | IContentProvider, ModrinthProvider |
| 任务管线 | ✅ 已完成 | TaskPipeline, TaskQueue |
| 预检服务 | ✅ 已完成 | PreflightService |
| 下载服务 | ✅ 已完成 | DownloadService |
| 设置服务 | ✅ 已完成 | SettingsService |
| 内容安装服务 | ✅ 已完成 | ContentInstallService |
| UI 页面框架 | ✅ 已完成 | 6个主页面 + 实例工作台 |

### 1.2 部分实现的组件

| 组件 | 状态 | 缺口 |
|------|------|------|
| 账户系统 | ⚠️ 部分 | 有接口，需完整实现 OAuth 流程 |
| Java 管理 | ⚠️ 部分 | 有服务，需实现自动发现/下载 |
| Loader 安装 | ⚠️ 部分 | 有接口，需实现各 Loader 安装器 |
| 备份服务 | ⚠️ 部分 | 有接口，需实现快照/回滚 |
| 诊断服务 | ⚠️ 部分 | 有框架，需实现日志解释 |

### 1.3 待实现的组件

| 组件 | 状态 | 优先级 |
|------|------|--------|
| Minecraft 版本下载 | ❌ 未实现 | P0 |
| 库文件下载与校验 | ❌ 未实现 | P0 |
| 启动命令构建 | ❌ 未实现 | P0 |
| 实例创建向导 | ❌ 未实现 | P1 |
| 拖拽安装 | ❌ 未实现 | P2 |
| 智能性能建议 | ❌ 未实现 | P3 |

---

## 二、详细缺口分析

### 2.1 账户与身份 (Auth)

dawn.md 要求：
- Microsoft 账号登录
- 离线档案
- 多账户切换
- 头像、昵称、UUID 缓存
- 账户失效检测与刷新

当前状态：
- `account_service.h` 和 `microsoft_oauth_service.h` 已定义
- 需要完整实现 OAuth 2.0 设备流
- 需要实现账户存储和刷新机制

### 2.2 Java 管理

dawn.md 要求：
- 自动发现本机 Java
- 自动下载匹配版本的 Java
- 每实例指定 Java
- 内存滑杆 + 专家模式参数
- GC 预设
- 启动前自检

当前状态：
- `java_service.h` 已定义
- 需要实现 Java 版本检测和匹配逻辑
- 需要实现 Java 下载器

### 2.3 Loader 安装

dawn.md 要求：
- Fabric / Quilt / Forge / NeoForge 安装
- OptiFine 特殊处理

当前状态：
- `loader_service.h` 和 `ILoaderInstaller` 接口已定义
- 需要实现各 Loader 的具体安装器

### 2.4 Minecraft 核心服务

dawn.md 要求：
- 原版版本管理
- Snapshot / Release / Legacy 分类
- 版本元数据下载
- 库文件和资源下载
- 启动参数构建

当前状态：
- `minecraft_service.h` 已定义
- 需要实现版本清单下载和解析
- 需要实现库文件下载和校验

### 2.5 实例详情页

dawn.md 要求 8 个 Tab：
1. 概览 - 实例基本信息和启动准备状态
2. 模组 - 模组管理、依赖解析、更新检查
3. 资源包 - 资源包安装、排序、启用
4. 光影 - 光影安装、切换
5. 世界与存档 - 世界列表、备份/还原
6. 日志 - 启动日志、崩溃日志、人话解释
7. 运行环境 - Java、内存、Loader 详情
8. 高级设置 - JVM 参数、环境变量

当前状态：
- 8 个 Tab 已在 `instance_workbench.h` 中定义
- UI 已在 `InstancesPage.qml` 中实现基础框架
- 需要完善各 Tab 的具体功能

---

## 三、实现路线图

### Phase 1: P0 核心功能 (立即执行)

1. **Minecraft 版本服务**
   - 实现版本清单下载 (launcher metadata)
   - 实现版本 JSON 解析
   - 实现库文件下载和校验

2. **启动运行时**
   - 实现 `DefaultLauncherRuntime`
   - 构建启动命令
   - 实现启动前预检

3. **Loader 安装器**
   - 实现 Fabric 安装器
   - 实现 Forge 安装器
   - 实现 NeoForge 安装器

### Phase 2: P1 增强功能

1. **账户系统**
   - 实现 Microsoft OAuth 设备流
   - 实现离线档案
   - 实现多账户切换

2. **Java 管理**
   - 实现 Java 自动发现
   - 实现 Java 下载

3. **实例创建向导**
   - 实现 4 步向导 UI
   - 集成版本和 Loader 选择

### Phase 3: P2 高级功能

1. **备份系统**
   - 实现快照功能
   - 实现回滚功能
   - 实现自动备份策略

2. **诊断服务**
   - 实现日志解释
   - 实现人话总结

3. **拖拽安装**
   - 实现文件拖拽检测
   - 实现自动分类和安装

### Phase 4: P3 优化体验

1. **智能建议**
   - 实现性能建议
   - 实现更新模拟

2. **动效优化**
   - 完善页面过渡动画
   - 优化交互反馈

---

## 四、文件创建计划

### 4.1 需要创建的新文件

```
core/
├── include/dawn/core/
│   ├── minecraft/
│   │   ├── version_manifest.h          # 版本清单模型
│   │   ├── version_package.h           # 版本包模型
│   │   └── library_resolver.h          # 库文件解析器
│   ├── loaders/
│   │   ├── fabric_installer.h          # Fabric 安装器
│   │   ├── forge_installer.h           # Forge 安装器
│   │   ├── neoforge_installer.h        # NeoForge 安装器
│   │   └── quilt_installer.h           # Quilt 安装器
│   └── auth/
│       └── offline_account_service.h   # 离线账户服务
└── src/
    ├── minecraft/
    │   ├── version_manifest.cpp
    │   ├── version_package.cpp
    │   └── library_resolver.cpp
    ├── loaders/
    │   ├── fabric_installer.cpp
    │   ├── forge_installer.cpp
    │   ├── neoforge_installer.cpp
    │   └── quilt_installer.cpp
    └── auth/
        └── offline_account_service.cpp
```

### 4.2 需要修改的现有文件

```
core/
├── src/
│   ├── minecraft/
│   │   └── minecraft_service.cpp       # 添加版本下载逻辑
│   ├── loaders/
│   │   └── loader_service.cpp          # 集成各 Loader 安装器
│   ├── auth/
│   │   ├── account_service.cpp         # 完善账户管理
│   │   └── microsoft_oauth_service.cpp # 实现 OAuth 流程
│   └── interfaces/
│       └── default_launcher_runtime.cpp # 实现启动命令构建
└── include/dawn/core/
    └── model/
        └── instance_manifest.h         # 添加缺失字段
```

---

## 五、技术规范

### 5.1 代码规范

- 使用 C++20 标准
- 遵循现有命名约定
- 所有 public 接口必须有文档注释
- 使用 `std::optional` 和 `std::expected` (C++23) 处理可能失败的返回值

### 5.2 错误处理

- 使用 `std::string* error` 参数传递错误信息
- 所有网络操作必须有超时和重试机制
- 文件操作必须处理 `std::error_code`

### 5.3 测试要求

- 所有新组件必须有单元测试
- 集成测试覆盖主要用户流程
- 使用 Google Test 框架

---

## 六、验收标准

### 6.1 P0 验收标准

- [ ] 可以下载任意 Minecraft 版本
- [ ] 可以安装 Fabric/Forge/NeoForge
- [ ] 可以构建有效的启动命令
- [ ] 启动前预检可以检测常见问题

### 6.2 P1 验收标准

- [ ] 可以登录 Microsoft 账户
- [ ] 可以创建离线档案
- [ ] 可以自动发现和下载 Java
- [ ] 实例创建向导可以完整运行

### 6.3 P2 验收标准

- [ ] 可以创建实例快照
- [ ] 可以回滚到之前的状态
- [ ] 日志可以显示人话解释
- [ ] 可以拖拽安装文件

### 6.4 P3 验收标准

- [ ] 根据实例类型给出性能建议
- [ ] 更新前显示影响分析
- [ ] 所有页面切换有流畅动画

---

## 七、附录

### 7.1 参考文档

- [Minecraft Launcher Metadata](https://launchermeta.mojang.com/mc/game/version_manifest.json)
- [Modrinth API Documentation](https://docs.modrinth.com/)
- [Microsoft OAuth 2.0 Device Flow](https://docs.microsoft.com/en-us/azure/active-directory/develop/v2-oauth2-device-code)

### 7.2 相关文件

- `dawn.md` - 产品规范文档
- `README.md` - 项目说明
- `CMakeLists.txt` - 构建配置
