# 轻量级 HIS 流式数据汇总层设计

## 1. 目标

针对当前桌面端 `dashboard refresh scans entire repositories every time` 的问题，引入一层 `可复用、只读、现算、绝对同步` 的数据汇总能力，用于替代“先 `load_all()` 再统计/截取”的读取模式。

本轮设计目标：

- 让管理员 dashboard 不再为计数和最近记录构造整份全量链表
- 抽出一层可复用的数据汇总能力，而不是只在 `DesktopAdapters` 内做一次性优化
- 保持 `txt repository` 架构不变
- 保持 `每次读取都现算`，不引入缓存和额外持久化
- 保持现有 UI 输出结构不变

## 2. 问题背景

当前 [DesktopAdapters_load_dashboard](/Users/cornna/project/his/his/src/ui/DesktopAdapters.c#L288) 会：

- 全量读取患者
- 全量读取挂号
- 全量读取住院记录
- 全量读取药品
- 全量读取发药记录

随后再进行：

- 计数
- 过滤
- 活跃状态判断
- 最近 5 条记录截取

这种模式在 demo 数据规模下可接受，但其时间和内存成本会随着记录规模线性放大，并且相同模式已经开始在其他只读查询场景中出现。

## 3. 范围

本轮设计覆盖：

- 新增 repository 级流式扫描接口
- 新增 service 级只读汇总层
- 重构 dashboard 读取路径接入汇总层
- 为后续“只读统计/最近记录/条件过滤”提供复用基础
- 新增相应单元测试和桌面适配层回归测试

本轮设计不覆盖：

- 写路径重构
- 缓存、索引、摘要文件或异步预计算
- 将所有现有 `load_all() + 统计` 查询一次性迁移完
- 改写底层文本存储格式
- 引入数据库或后台服务

## 4. 方案选择

### 4.1 备选方案

#### 方案 A：仅在 UI 层局部优化

只修改 `DesktopAdapters_load_dashboard()` 内部实现，把重复逻辑拆成几个 helper。

优点：

- 改动最小
- 风险最低

缺点：

- 不能复用
- 其他页面仍会继续复制“全量加载再统计”的模式

#### 方案 B：新增可复用流式汇总层

在 repository 之上新增一层只读汇总服务，repository 负责逐条解析，汇总层负责统计、过滤、截取最近 N 条。

优点：

- 满足复用目标
- 保持读取绝对同步
- 解析职责与汇总职责边界清晰

缺点：

- 需要新增接口和测试
- 初次接线比局部优化稍大

#### 方案 C：把统计接口分散下沉到各个 repository

在每个 repository 单独增加 `count_*`、`find_recent_*` 一类接口。

优点：

- 贴近数据源

缺点：

- 容易碎片化
- 组合型查询仍需要上层拼装
- 难形成统一复用模型

### 4.2 设计结论

采用 `方案 B：新增可复用流式汇总层`。

## 5. 总体设计

### 5.1 分层结构

新增两类能力：

- `repository` 层新增 `for_each(...)` 形式的流式扫描接口
- `service` 层新增 `SummaryService`

职责划分如下：

- repository：
  - 保持对文本文件格式的所有权
  - 负责逐行读取、字段拆分、领域对象解析
  - 对外暴露只读扫描能力
- `SummaryService`：
  - 负责计数、状态判断、最近 N 条窗口维护、跨 repository 结果组装
  - 不负责文本格式解析
  - 不负责写入和缓存
- `DesktopAdapters`：
  - 改为调用 `SummaryService_load_dashboard()`
  - 不再自行管理五份全量读取和二次遍历

### 5.2 推荐目录

- `/Users/cornna/project/his/his/include/service/SummaryService.h`
- `/Users/cornna/project/his/his/src/service/SummaryService.c`
- `/Users/cornna/project/his/his/tests/test_summary_service.c`

## 6. Repository 流式扫描接口

### 6.1 接口原则

每个需要参与汇总的 repository 增加一类统一风格的只读扫描接口：

- `PatientRepository_for_each(...)`
- `RegistrationRepository_for_each(...)`
- `AdmissionRepository_for_each(...)`
- `MedicineRepository_for_each(...)`
- `DispenseRecordRepository_for_each(...)`

统一原则：

- 读取顺序与文件存储顺序一致
- 逐条解析为临时领域对象后交给 visitor
- 不构造全量 `LinkedList`
- visitor 返回失败时立即中止扫描
- 非法行和存储错误继续沿用现有 repository 失败语义

### 6.2 visitor 形态

建议统一为：

- `typedef Result (*XxxRepositoryVisitor)(const Xxx *record, void *context);`

扫描函数形式：

- `Result XxxRepository_for_each(const XxxRepository *repository, XxxRepositoryVisitor visitor, void *context);`

这样能够：

- 贴合当前 `Result` 风格
- 支持正常中止和异常传播
- 避免为每类查询设计过多专用接口

### 6.3 与现有 `load_all()` 的关系

本轮不删除现有 `load_all()`。

保留原因：

- 现有业务逻辑和测试仍依赖这些接口
- 本轮目标是补充读取模式，而不是大规模替换所有历史路径

后续若某些热点路径也迁移到流式模式，再逐步收缩 `load_all()` 的使用面。

## 7. SummaryService 设计

### 7.1 核心职责

`SummaryService` 提供组合型只读聚合查询。

首批落地接口：

- `SummaryService_init(...)`
- `SummaryService_load_dashboard(..., DesktopDashboardState *out_dashboard)`

其中 `SummaryService_load_dashboard()` 负责填充：

- `patient_count`
- `registration_count`
- `inpatient_count`
- `low_stock_count`
- `recent_registrations`
- `recent_dispensations`

### 7.2 依赖方式

`SummaryService` 不拥有 repository 生命周期，只持有已初始化 repository 的只读引用，或在初始化阶段接收包含这些 repository 的 service/application 上下文。

推荐优先选择：

- 初始化时接收当前已经存在的 repository/service 引用

不推荐：

- 在 `SummaryService` 内部再次按 path 重新初始化 repository

原因：

- 避免重复初始化
- 降低状态分散
- 更贴合现有 `MenuApplication` / `DesktopAdapters` 的对象关系

### 7.3 Dashboard 查询实现

`SummaryService_load_dashboard()` 的实现逻辑：

1. 清空输出结构
2. 扫描患者 repository，累计患者总数
3. 扫描挂号 repository，累计挂号总数，并维护最近 5 条挂号窗口
4. 扫描住院 repository，仅统计 `ADMISSION_STATUS_ACTIVE`
5. 扫描药品 repository，统计低库存药品数
6. 扫描发药 repository，维护最近 5 条发药窗口
7. 将结果写入 `DesktopDashboardState`

### 7.4 最近 N 条窗口策略

最近记录不再通过：

- 全量读入
- 计算总数
- 截取尾部
- 再倒序复制

替代为固定大小滚动窗口：

- 扫描过程中最多保留 `N=5` 条记录
- 新记录进入后，如果超过容量，则淘汰最早一条
- 输出阶段按当前 UI 需要的顺序复制到 `LinkedList`

该策略将最近记录维护的空间复杂度从 `O(total)` 降到 `O(N)`。

### 7.5 低库存统计策略

低库存统计不再依赖完整药品列表。

扫描药品 repository 时：

- 对每条药品记录直接判断是否处于低库存状态
- 命中则计数加一

只保留计数结果，不构造中间完整集合。

## 8. DesktopAdapters 接入方式

### 8.1 dashboard 重构

`DesktopAdapters_load_dashboard()` 从“多 repository 全量加载协调者”改为“汇总层调用者”。

改造后职责：

- 参数校验
- 清空 dashboard 输出
- 调用 `SummaryService_load_dashboard()`
- 返回结果

不再保留当前分散在函数内部的：

- 多次 `load_all()`
- 多次 `LinkedList_count()`
- 最近记录翻转复制
- 活跃住院遍历统计

### 8.2 行为兼容要求

UI 观察到的行为必须保持不变：

- 字段名不变
- 最近记录展示顺序不变
- 计数语义不变
- 错误传播风格不变

本轮改造是性能与结构优化，不是 dashboard 行为变更。

## 9. 对后续复用的支撑

本轮虽然只接 dashboard，但设计上要支持后续迁移以下读取模式：

- 时间范围统计
- 只读列表摘要
- 活跃状态统计
- 最近记录窗口
- 按条件过滤但不需要全量物化的查询

本轮不直接改造这些页面，但接口命名和抽象方式要为它们留出空间。

## 10. 测试设计

### 10.1 repository 扫描测试

为新增的 `for_each(...)` 接口补单测，覆盖：

- 空文件
- 只有表头
- 多条记录正常遍历
- visitor 中止
- 非法行解析失败

### 10.2 SummaryService 测试

新增 `tests/test_summary_service.c`，覆盖：

- 患者总数统计正确
- 挂号总数统计正确
- 活跃住院数只统计 `ACTIVE`
- 低库存药品数正确
- 最近 5 条挂号顺序正确
- 最近 5 条发药顺序正确
- 少于 5 条时边界行为正确

### 10.3 DesktopAdapters 回归测试

保留并调整现有 desktop adapter / dashboard 相关测试，确保：

- 输出结构保持一致
- 新汇总层接入后行为不变

### 10.4 TDD 要求

实现顺序必须遵循：

1. 先写 repository `for_each(...)` 的失败测试
2. 再写最小实现
3. 再写 `SummaryService` 的失败测试
4. 再接 dashboard
5. 最后做回归整理

## 11. 实施顺序

### 11.1 第一阶段

- 为 5 个 repository 增加只读扫描接口
- 补对应单测

### 11.2 第二阶段

- 新增 `SummaryService`
- 为 dashboard 汇总逻辑补单测

### 11.3 第三阶段

- 将 `DesktopAdapters_load_dashboard()` 切换到 `SummaryService`
- 修复并更新回归测试

### 11.4 第四阶段

- 评估后续热点读取路径是否值得迁移
- 不在本轮与 dashboard 优化一起强绑

## 12. 风险与约束

### 12.1 主要风险

- 新增 `for_each(...)` 时复制了解析逻辑，可能和现有 `load_all()` 语义漂移
- 最近 N 条窗口若顺序处理错误，会造成 UI 展示顺序回归
- 若 `SummaryService` 过早承载太多查询，可能演变为新的大文件

### 12.2 控制策略

- 优先复用现有 repository 内部解析函数
- 在测试里明确锁定最近记录顺序
- 本轮只接 dashboard，不把所有摘要查询一次性迁入
- 若 `SummaryService` 职责增长过快，后续再拆成 dashboard summary / history summary

## 13. 验收标准

满足以下条件即可视为本轮完成：

- dashboard 不再通过五份全量 `load_all()` 完成统计
- 新增汇总层可复用，不是 `DesktopAdapters` 内部私有技巧
- dashboard 行为对用户保持一致
- 新增 repository 扫描接口通过单测
- `SummaryService` 通过单测
- desktop adapter 回归测试通过

## 14. 结论

本轮采用 `repository 流式扫描接口 + service 级可复用汇总层` 的方案，先解决 dashboard 的全量加载问题，同时为后续只读摘要查询提供统一基础设施。

该方案在不引入缓存、不改变存储格式、不修改写路径的前提下，实现：

- 读取绝对同步
- 更低的中间内存占用
- 更清晰的职责边界
- 后续可持续复用
