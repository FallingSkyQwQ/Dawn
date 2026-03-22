用到的UI库：https://github.com/zhuzichu520/FluentUIbi
Dawn 是：

不是“又一个 MC 启动器”，而是一个 以实例为核心、以内容管理为工作流、以 Fluent Design 为第一视觉语言 的现代桌面启动器。
核心目标只有三条：
	1.	比传统启动器更清楚：任何资源、版本、Loader、Java、存档都明确归属到某个实例。
	2.	比大多数 Fluent 风格软件更克制：不靠乱堆毛玻璃和阴影，而靠层级、节奏、留白、反馈和动效统一来赢。
	3.	比“功能全但难用”的启动器更顺手：90% 常用操作三步内完成，专家功能不删，但默认折叠。

。zhuzichu520/FluentUI 的主分支支持 Qt 6，Qt 5 需要切到单独分支；它用 CMake 构建，组件里已经有 FluApp/Router、FluWindow、FluNavigationView、FluInfoBar、FluContentDialog、FluProgressBar/ProgressRing、FluTreeView、FluTableView、FluCarousel、FluTheme、FluMediaPlayer、Date/TimePicker 等，够做出完整的 launcher shell。要注意的是 README 现在还写了“更推荐用 FluentUI Pro”，以及 FluWindow 的无边框窗口能力是 Windows-only，所以 Dawn 应该做成 Windows 优先体验、跨平台可运行但窗口外壳分级适配。 ￼

Dawn 的产品定位

一句话定义：

Dawn = 实例管理器 + 内容中心 + 启动编排器 + 诊断修复器

不是只会“下载一个版本然后启动”，而是：
	•	用 实例 承载一切
	•	用 工作流 组织复杂操作
	•	用 可视反馈 让下载、修复、冲突、依赖都透明
	•	用 Fluent 设计语义 做出真正现代、优雅、耐看的桌面体验

⸻

一、信息架构

顶层导航

用 FluNavigationView 做主导航，左侧固定 6 个一级入口：
	•	首页
	•	实例
	•	内容中心
	•	下载队列
	•	日志与修复
	•	设置

二级页通过 FluApp 路由切换，保持单窗口 SPA 体验。 ￼

页面职责

首页
	•	最近启动实例
	•	今日推荐内容
	•	Java/资源完整性状态
	•	下载队列摘要
	•	系统公告 / 更新提示

实例
	•	实例网格 / 列表双视图
	•	创建、复制、导入、导出、备份、修复
	•	每个实例进入详情工作台

内容中心
	•	接入 Modrinth
	•	搜索 mod / modpack / resourcepack / shader
	•	筛选 loader、游戏版本、客户端/服务端适配
	•	一键安装到指定实例

下载队列
	•	统一显示所有下载、解压、校验、安装任务
	•	支持暂停、恢复、重试、并发调度

日志与修复
	•	启动日志、崩溃日志、依赖缺失、Java 不匹配、资源损坏
	•	一键修复和问题解释

设置
	•	主题、窗口、下载、缓存、Java、账户、实验功能、备份策略

⸻

二、实例是 Dawn 的核心

实例模型

每个实例都应当是独立、可移植、可快照的。

一个实例包含：
	•	Minecraft 版本
	•	Loader 类型与版本
	•	可选 OptiFine
	•	Java 运行时配置
	•	独立 mods / resourcepacks / shaderpacks / saves / logs / config
	•	启动参数
	•	图标、配色、标签、备注
	•	锁定文件（记录资源来源和已安装版本）
	•	备份与恢复点

实例视图

实例卡片需要显示：
	•	图标 / Banner
	•	名称
	•	MC 版本
	•	Loader 徽标
	•	Java 状态
	•	资源数量
	•	最近启动时间
	•	健康状态

实例详情页

实例详情不是一堆设置，而是一个工作台，建议拆成 8 个 Tab：
	•	概览
	•	模组
	•	资源包
	•	光影
	•	世界与存档
	•	日志
	•	运行环境
	•	高级设置

这样用户不会在一个巨大的设置页里迷路。

⸻

三、必须支持的功能范围

你说“绝大部分 MC 启动器都有的功能”，那 Dawn 至少要覆盖到这一层：

1. 账户与身份
	•	Microsoft 账号登录
	•	离线档案
	•	多账户切换
	•	头像、昵称、UUID 缓存
	•	账户失效检测与刷新

2. 版本与实例
	•	原版版本管理
	•	Snapshot / Release / Old Alpha/Beta 分类
	•	实例创建、复制、克隆、导入、导出
	•	实例隔离目录
	•	每实例独立参数、分辨率、内存、Java

3. Loader
	•	Fabric
	•	Quilt
	•	Forge
	•	NeoForge
	•	OptiFine（见后面的特殊说明）

4. 内容管理
	•	模组安装/卸载/启用/禁用
	•	材质包安装/排序/启用
	•	光影安装/切换
	•	依赖自动拉取
	•	冲突预警
	•	更新检查
	•	版本回滚

5. Java 管理
	•	自动发现本机 Java
	•	自动下载匹配版本的 Java
	•	每实例指定 Java
	•	内存滑杆 + 专家模式参数
	•	GC 预设
	•	启动前自检

6. 下载与修复
	•	多线程下载
	•	断点续传
	•	SHA 校验
	•	缺失文件重拉
	•	资源索引修复
	•	库文件修复
	•	日志归档

7. 存档与备份
	•	世界列表
	•	备份/还原
	•	启动前自动备份
	•	大更新前快照
	•	导出 zip

8. 可用性增强
	•	最近任务回退
	•	拖拽安装本地 jar/zip
	•	清理缓存
	•	低磁盘空间警告
	•	首次启动向导
	•	新手/高级双模式

⸻

四、Modrinth 集成应该怎么做

Modrinth 这块不该做成“内嵌网页”，而该做成 Dawn 原生内容体验。

Modrinth 官方 API 现在支持项目搜索，搜索可按 project_type、categories（搜索里 loader 也归在 categories）、versions 等 facets 过滤；有效的项目类型包括 mod、modpack、resourcepack、shader；项目版本接口也支持按 loaders 和 game_versions 过滤；依赖还能通过单独接口取回。也就是说，Dawn 完全可以原生做出面向实例的检索、版本匹配、依赖解析和安装流程，而不是把网页塞进去。 ￼

Dawn 里的 Modrinth 工作流

搜索页
顶部：
	•	搜索框
	•	项目类型切换：模组 / 整合包 / 材质包 / 光影
	•	游戏版本筛选
	•	Loader 筛选
	•	客户端/服务端适配
	•	排序切换

中间：
	•	卡片流结果
	•	卡片内容：图标、标题、作者、简介、下载量、更新时间、支持版本、兼容 Loader

右侧：
	•	详情抽屉
	•	版本选择
	•	依赖树
	•	安装目标实例

安装逻辑
	•	mod → 装进目标实例的 mods
	•	resourcepack → 装进 resourcepacks
	•	shader → 装进 shaderpacks
	•	modpack → 新建实例并按 pack 清单部署

安装前解析
	•	当前实例的 MC 版本
	•	当前实例 Loader
	•	是否已有相同项目
	•	依赖是否缺失
	•	是否存在已知冲突
	•	是否需要升级 Loader / Java

安装反馈
不要只弹一句“安装成功”。
要给出：
	•	安装了什么
	•	自动补了哪些依赖
	•	跳过了什么
	•	哪些资源需要下次重启实例生效

⸻

五、Loader 与实例创建工作流

创建实例向导

Dawn 最重要的一页之一就是 新建实例向导。

流程我建议分 4 步：

第 1 步：选择基础
	•	原版
	•	从 Modrinth 整合包创建
	•	从本地整合包导入
	•	复制现有实例

第 2 步：选择版本
	•	MC 版本
	•	类型：Release / Snapshot / Legacy

第 3 步：选择 Loader
	•	无
	•	Fabric
	•	Quilt
	•	Forge
	•	NeoForge
	•	OptiFine（可选）

第 4 步：实例预设
	•	图标
	•	名称
	•	内存
	•	Java
	•	推荐性能包
	•	推荐基础模组
	•	是否启用启动前备份

自动下载实例与 Loader

Minecraft 官方提供了 launcher metadata 和各版本 package json，可作为 Dawn 的权威安装元数据来源；这些元数据里能拿到平台相关的启动器/运行时信息，而具体版本 json 里能拿到启动参数、资源目录、库等信息，所以 Dawn 完全可以把“创建实例 → 拉取版本元数据 → 拉取库与资源 → 配置运行参数”做成标准化管线。 ￼

⸻

六、OptiFine 该怎么支持

这块别做幼稚。

OptiFine 的公开下载主要是网页形式，官方页面会列出版本、镜像入口和对应 Forge 兼容信息，但它不像 Modrinth 那样给你一套干净的内容分发 API。换句话说，OptiFine 适合做成“特例适配器”而不是“统一 Provider”。 ￼

所以 Dawn 里正确的做法是：
	•	支持 OptiFine standalone 实例
	•	支持 Forge + OptiFine 的兼容组合（仅在明确兼容时显示）
	•	若用户选了 Fabric/Quilt，优先提示性能/光影替代路线，而不是硬塞 OptiFine
	•	下载策略做成两级：
	•	自动镜像安装
	•	用户手动提供 jar 安装
	•	所有 OptiFine 安装流程都要标注：
	•	来源
	•	兼容版本
	•	可回退
	•	失败原因

这比“看起来一键，实际上经常炸”的实现专业得多。

⸻

七、UI/UX：要比很多所谓 Fluent 软件更像 Fluent

1. 视觉原则

Dawn 的 Fluent 不是“贴皮”，而是这几条：
	•	层级先于装饰
	•	内容先于容器
	•	状态先于炫技
	•	动效服务于理解，不服务于表演

2. 界面气质

整体风格建议：
	•	深浅双主题
	•	默认深色更沉浸
	•	高亮色可跟随实例主题色
	•	卡片圆角统一，不混乱
	•	毛玻璃只出现在顶层导航、抽屉、上下文菜单、对话框
	•	主工作区域尽量保持纯净实色背景
	•	强调“晨曦感”：柔和、清澈、低饱和，不要赛博夜店风

3. 动效原则
	•	页面切换：短距离滑入 + 淡入
	•	卡片 hover：轻微抬升 + 阴影变化
	•	列表更新：高度过渡，不闪烁重排
	•	安装成功：局部高亮，不整页庆祝
	•	失败反馈：就地展开原因，不要只弹错误码

动画时长建议：
	•	微交互：120–160ms
	•	面板切换：180–220ms
	•	页面过渡：240–300ms

4. 交互密度

做两个密度模式：
	•	标准模式：适合普通用户
	•	紧凑模式：适合重度玩家/整合包作者

5. 新手与高级双轨

任何复杂功能都做成：
	•	默认只给结论和推荐值
	•	展开“高级设置”后再给 JVM 参数、环境变量、类路径覆盖、下载源策略等

⸻

八、把 FluentUI 真的“用满”，但不要滥用

这个库里现成的组件不少，Dawn 应该这样用：
	•	FluApp：整站 Router 与全局状态入口
	•	FluWindow + FluAppBar：主窗口壳层
	•	FluNavigationView：一级导航
	•	FluInfoBar：下载完成、修复结果、兼容提醒、更新提示
	•	FluContentDialog：删除实例、覆盖安装、回滚确认
	•	FluProgressBar / FluProgressRing：下载、解压、修复、首次初始化
	•	FluTreeView：依赖树、日志分类、实例文件树
	•	FluTableView：版本列表、下载队列、日志表
	•	FluCarousel：首页推荐整合包 / 最近实例
	•	FluFlipView：截图浏览 / 世界预览
	•	FluSlider：内存、并发下载数、UI 缩放
	•	FluDatePicker / FluTimePicker：自动备份计划、静默更新窗口
	•	FluTooltip：高级选项解释
	•	FluTheme：主题色、暗色、跟随系统
	•	FluMediaPlayer：仅用于欢迎页/功能介绍短视频，别污染核心工作流
	•	FluScrollbar / Pagination：大列表与搜索结果体验优化  ￼

重点是：
不是为了“用了所有组件”而硬塞功能，而是把组件变成清晰的交互语义。

⸻

九、技术架构

总体结构

前端：QML + FluentUI
后端：C++ 服务层
模式：MVVM + Service + Task Pipeline

模块拆分

Dawn/
  app/
    main.cpp
    bootstrap/
  ui/
    qml/
      shell/
      pages/
      components/
      dialogs/
    viewmodels/
  core/
    auth/
    instance/
    minecraft/
    loaders/
    content/
    modrinth/
    java/
    download/
    diagnostics/
    backup/
    settings/
  infra/
    net/
    fs/
    archive/
    hash/
    process/
    db/

关键层级

UI 层
	•	纯展示
	•	不直接碰网络和文件系统
	•	通过 ViewModel 绑定数据

ViewModel 层
	•	给 QML 暴露属性、命令、状态
	•	负责页面逻辑组合
	•	不处理底层安装细节

Service 层
	•	实例服务
	•	版本安装服务
	•	内容安装服务
	•	Java 服务
	•	修复服务
	•	日志服务

Pipeline 层
所有下载/安装/修复动作统一进任务管线：
	•	解析任务
	•	下载任务
	•	校验任务
	•	解压任务
	•	部署任务
	•	回滚任务
	•	结果汇总

数据存储建议
	•	SQLite：全局配置、账户、下载历史、任务记录
	•	JSON：实例清单、锁文件、导出清单
	•	实例目录本地文件：真正的游戏资源、模组、配置、世界存档

⸻

十、建议的数据模型

全局配置
	•	主题
	•	下载并发
	•	缓存路径
	•	默认 Java 策略
	•	默认内存策略
	•	备份策略
	•	网络代理
	•	实验功能开关

InstanceManifest
	•	id
	•	name
	•	icon
	•	mcVersion
	•	loaderType
	•	loaderVersion
	•	optifineVersion
	•	javaProfileId
	•	memoryProfile
	•	gameDir
	•	createdAt
	•	lastPlayedAt
	•	tags
	•	notes

ContentLock
	•	provider
	•	projectId
	•	versionId
	•	fileHash
	•	installedPath
	•	enabled
	•	dependencies[]

这个锁文件很重要。
没有它，你后面做更新检查、依赖回收、冲突分析都会越来越乱。

⸻

十一、几个非常值钱的“实用功能”

这些功能最能把 Dawn 拉开档次：

1. 启动前预检

在点“启动”之前自动检查：
	•	Java 是否匹配
	•	资源是否缺失
	•	Loader 是否完整
	•	关键依赖是否断裂
	•	存储空间是否足够

2. 一键修复

不是“重新下载全部”，而是：
	•	缺哪个补哪个
	•	哪个 hash 不对修哪个
	•	哪个配置明显错了给解释

3. 智能性能建议

根据实例类型给建议：
	•	纯净版
	•	轻量整合
	•	大型整合包
	•	光影实例

4. 更新模拟

更新模组前先告诉用户：
	•	将升级哪些项目
	•	哪些依赖会变化
	•	哪些可能不兼容
	•	可否自动回滚

5. 启动日志解释

别只给 raw log。
要给“人话总结”：
	•	Java 版本不对
	•	模组缺依赖
	•	Loader 版本冲突
	•	配置文件损坏
	•	显卡驱动 / OpenGL 问题

6. 实例快照

对整合包玩家特别重要。
	•	更新前自动快照
	•	一键回退

7. 资源拖拽安装

把 jar / zip 直接拖进窗口：
	•	自动识别 mod / resourcepack / shader / modpack
	•	自动问你装到哪个实例

⸻

十二、代码层建议：关键接口先抽象

别一开始就把逻辑写死在 Modrinth 或某个 Loader 上。

class IContentProvider {
public:
    virtual SearchResult search(const SearchQuery& query) = 0;
    virtual VersionList getVersions(const QString& projectId) = 0;
    virtual DependencyGraph resolveDependencies(const InstallRequest& req) = 0;
    virtual TaskPlan buildInstallPlan(const InstallRequest& req) = 0;
    virtual ~IContentProvider() = default;
};

class ILoaderInstaller {
public:
    virtual QList<LoaderVersion> listVersions(const QString& mcVersion) = 0;
    virtual TaskPlan buildInstallPlan(const LoaderInstallRequest& req) = 0;
    virtual ~ILoaderInstaller() = default;
};

class ILauncherRuntime {
public:
    virtual PreflightResult preflight(const LaunchRequest& req) = 0;
    virtual LaunchCommand buildCommand(const LaunchRequest& req) = 0;
    virtual ~ILauncherRuntime() = default;
};

这样后面你想加：
	•	CurseForge
	•	本地包导入
	•	第三方镜像
	•	自定义 loader

都不会把架构打烂。

⸻

十三、开发优先级

P0：先把骨架做对
	•	主窗口
	•	路由
	•	主题系统
	•	实例列表
	•	创建原版实例
	•	启动原版
	•	下载队列

P1：把 Dawn 变成“能用的启动器”
	•	Java 管理
	•	多账户
	•	Loader 安装
	•	实例详情页
	•	日志与修复

P2：把 Dawn 变成“强力启动器”
	•	Modrinth 搜索
	•	依赖解析
	•	一键装模组/材质/光影到实例
	•	更新检查
	•	快照回滚

P3：把 Dawn 变成“顶级体验”
	•	智能建议
	•	拖拽安装
	•	动效细化
	•	欢迎引导
	•	跨平台打磨
	•	可视化冲突诊断

⸻

十四 Dawn 的最终调性结论

Dawn 不该像很多 MC 启动器那样：
	•	功能很多，但像工具箱
	•	页面很多，但没有信息秩序
	•	样式很花，但不耐看
	•	会下载东西，但不解释发生了什么

Dawn 应该是：

冷静、清楚、现代、实例导向、内容导向、恢复能力强。
