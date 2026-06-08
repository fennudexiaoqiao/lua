# 工作包 B：Lua 超集与 XML UI 绑定的源码级改造方案

## 1. 目标

本文档承接 `BINDING_WORK_PACKAGE_A.md`，将“XML UI 内完整嵌入 Lua 超集”的语言设计落到 Lua 源码级改造任务。

与上一轮“Lua 子集表达式引擎”不同，本方案要求：

- 支持完整 Lua 脚本块。
- 支持 XML 属性内联 Lua 表达式。
- 支持 Lua 超集语法中的 `bind`、`var ... bind`、`var ... <-`。
- 支持绑定变量和变量绑定。
- 支持绑定区域静态依赖提取。
- 支持普通 Lua 区域与响应式绑定区域混合开发。

## 2. 总体实现路线

推荐采用“完整 Lua + 绑定扩展前端 + 独立绑定元数据”的路线。

核心原则：

- Lua VM 继续执行普通 Lua bytecode。
- 绑定语法在 lexer/parser/codegen 前端扩展。
- 绑定依赖、绑定变量、可写目标、类型信息生成独立 metadata。
- XML 编译器负责把 XML 属性内联表达式和 `<lua:script>` 转交给 Lua 超集编译器。
- 响应式调度器不放进 `lvm.c` 主循环，而在宿主 runtime 中运行。

## 3. XML 编译侧新增工作

Lua 仓库外的 XML UI 编译器需要完成：

- 识别 `${ ... }` 属性内联表达式。
- 识别 `<lua:script>` 脚本块。
- 识别 `<lua:include src="..." />` 外部 Lua 文件。
- 为每个 XML 对象分配稳定 `ui` 路径。
- 将 XML 位置信息传入 Lua 超集编译器。
- 将 Lua 编译结果中的绑定 metadata 合并到 UI artifact。

虽然这些不一定在 Lua 仓库实现，但它们是整体方案必需输入。

## 4. Lua 源码现有模块改造清单

## 4.1 `llex.h` / `llex.c`

### 目标

支持 Lua 超集新增 token。

### 必做改造

新增关键字或上下文关键字：

- `bind`
- `var`
- `mode`
- `twoway`
- `oneway`
- `converter`
- `trigger`
- `debounce`

新增操作符 token：

- `<-`：单向绑定或派生变量。
- `<=>`：双向绑定。
- `:`：类型标注可复用现有字符 token。

### 设计建议

`bind` 和 `var` 建议作为保留字处理，因为它们是语言级声明。

`mode`、`converter`、`trigger`、`debounce` 建议作为上下文关键字处理，避免过度破坏普通 Lua 标识符兼容性。

### 输出

- 新 token 枚举。
- 新 token 字符串。
- scanner 识别 `<-` 与 `<=>`。

## 4.2 `lparser.h` / `lparser.c`

### 目标

解析完整 Lua + LBS 绑定扩展。

### 必做改造

新增语法入口：

- `bindstat`：绑定声明。
- `varbindstat`：绑定变量声明。
- `derivedvarstat`：派生变量声明。
- `xmlattrexpr`：XML 属性内联表达式入口。

新增解析能力：

- `bind target <- expr`
- `bind target <=> source`
- `var name: Type bind source [mode ...]`
- `var name: Type <- expr`
- `converter name`
- `trigger name`
- `debounce duration`

### 绑定声明示例

```lua
bind ui.label.text <- speedText
bind ui.slider.value <=> motorSpeed
var motorSpeed: INT bind extern.plc.motor.speed mode twoway
var speedText: STRING <- format('%d rpm', motorSpeed)
```

### 关键要求

普通 Lua 语法仍然完整保留。只有 `bind` / `var` 扩展声明进入新的解析分支。

## 4.3 `lparser.h` 中 AST/表达式描述扩展

Lua 原生 `expdesc` 偏向直接代码生成，不足以表达绑定 metadata。需要新增绑定专用结构，而不是把所有字段塞入 `expdesc`。

建议新增：

- `BindPath`
- `BindTarget`
- `BindDependency`
- `BindVarDecl`
- `BindDecl`
- `DerivedVarDecl`

这些结构可以放入新文件 `lbind.h`，由 `lparser.c` 调用。

## 4.4 `lcode.h` / `lcode.c`

### 目标

继续为普通 Lua 和绑定表达式生成 Lua bytecode，同时导出绑定 metadata。

### 必做改造

- `bind` 声明本身不应编译成普通运行语句。
- `bind` 应生成 binding metadata。
- `var ... bind` 应生成绑定变量 metadata，并按需要生成 Lua 侧代理变量。
- `var ... <- expr` 应生成派生变量 metadata。
- 普通 Lua 函数、事件处理器仍按原 Lua codegen 生成 bytecode。

### 关键设计

绑定声明是“声明式元信息”，不是普通 Lua 运行时语句。它应该进入 artifact / binding graph，而不是每次脚本执行时重新声明。

## 4.5 `lopcodes.h` / `lopcodes.c`

### 第一阶段策略

第一阶段不新增 opcode。

理由：

- 绑定声明先作为 metadata 输出。
- UI/PLC 路径读写通过宿主代理对象实现。
- 响应式调度在宿主 runtime 层完成。

### 第二阶段可选优化

如性能不足，可新增：

- `OP_GETSLOT`
- `OP_SETSLOT`
- `OP_GETEXTERN`
- `OP_SETEXTERN`

这些 opcode 只能在 slot 表稳定后引入。

## 4.6 `lvm.c`

### 第一阶段策略

保持 VM 主循环基本不变。

### 必做外围能力

- 支持受控全局环境。
- 支持宿主代理对象读写。
- 支持执行预算 hook。
- 支持错误映射回 XML/Lua 源码位置。

### 不建议做法

不要在 `lvm.c` 中硬编码 UI、PLC、extern、binding graph 逻辑。

## 4.7 `lapi.c` / `lua.h`

### 目标

提供 LBS 编译与执行 API。

### 建议新增 API

```c
lua_LBSCompileResult *luaLBS_compileXMLScript(lua_State *L, const char *source, const char *sourceName);
lua_LBSCompileResult *luaLBS_compileInlineExpr(lua_State *L, const char *expr, const char *sourceName);
void luaLBS_freeCompileResult(lua_State *L, lua_LBSCompileResult *result);
```

### 编译结果包含

- Lua Proto / closure。
- 绑定声明列表。
- 绑定变量列表。
- 派生变量列表。
- 依赖图。
- 诊断列表。

## 4.8 `lauxlib.c` / `lauxlib.h`

### 目标

提供宿主友好的高级 API。

### 建议新增

- `luaL_lbsloadscript`
- `luaL_lbsloadinlineexpr`
- `luaL_lbsopenlibs`
- `luaL_lbsdiagnostics`

## 4.9 `linit.c`

### 目标

支持 LBS 沙箱标准库。

### 必做改造

新增：

- `luaL_openlbslibs(lua_State *L)`

默认只开启：

- base 安全集合。
- math。
- string。
- table 的受限集合。

默认禁用：

- io。
- os。
- debug。
- package。
- coroutine。

## 5. 建议新增文件

## 5.1 `lbind.h` / `lbind.c`

定义绑定核心数据结构：

- `LBS_BindPath`
- `LBS_BindTarget`
- `LBS_BindDecl`
- `LBS_BindVarDecl`
- `LBS_DerivedVarDecl`
- `LBS_Dependency`
- `LBS_CompileMetadata`

## 5.2 `lbindparse.h` / `lbindparse.c`

承接 `lparser.c` 中的绑定扩展解析逻辑，避免继续膨胀主 parser 文件。

职责：

- 解析绑定声明尾部参数。
- 解析类型标注。
- 解析绑定路径。
- 构造绑定 AST / metadata。

## 5.3 `lbindsem.h` / `lbindsem.c`

绑定语义分析：

- 路径归一化。
- 静态依赖提取。
- 可写目标检查。
- 双向绑定可逆性检查。
- 类型检查。
- converter 检查。

## 5.4 `lbindxml.h`

定义 Lua 编译器与 XML UI 编译器之间的数据交换结构。

职责：

- XML 对象路径映射。
- XML 属性源码位置。
- `<lua:script>` 源码块信息。
- `${...}` 内联表达式位置信息。

## 5.5 `lbindhost.h`

定义宿主运行时接口：

- 读 UI 属性。
- 写 UI 属性。
- 读写 extern 点位。
- 读写 state。
- 注册 converter。
- 注册白名单函数。

## 6. 编译产物设计

LBS 编译器输出不只是 Lua bytecode，还必须输出绑定元数据。

建议结构：

```c
typedef struct LBS_CompileResult {
  int success;
  LClosure *closure;
  LBS_CompileMetadata *metadata;
  LBS_Diagnostic *diagnostics;
  int diagnosticCount;
} LBS_CompileResult;
```

metadata 至少包含：

- binding declarations。
- bound variable declarations。
- derived variables。
- dependency graph。
- writable targets。
- converter references。
- XML source mapping。

## 7. 混合开发运行模型

### 7.1 加载阶段

1. XML 编译器解析 UI 对象树。
2. 提取 XML 内联表达式和 Lua 脚本块。
3. 调用 LBS 编译器。
4. LBS 输出 Lua closure 与 binding metadata。
5. 宿主 runtime 建立 slot 表、依赖图和调度器。

### 7.2 运行阶段

1. 普通 Lua 脚本按事件触发执行。
2. 绑定变量通过宿主代理读写。
3. 外部点位变化触发依赖图。
4. 调度器重算派生变量和 UI 绑定。
5. 双向绑定由写回通道处理。

## 8. 第一阶段实施顺序

### B1：词法与语法骨架

- 增加 `bind`、`var`、`<-`、`<=>`。
- 解析绑定声明和绑定变量声明。
- 暂不生成完整依赖图。

### B2：绑定 metadata 输出

- 新增 `lbind.*`。
- 输出绑定声明列表。
- 输出变量绑定列表。
- 输出派生变量列表。

### B3：依赖提取

- 提取 `ui/state/extern/vm` 路径。
- 建立变量依赖关系。
- 对动态路径报诊断。

### B4：XML 内联表达式入口

- 支持 `${...}` 转换为匿名绑定表达式。
- 记录 XML 属性位置。

### B5：沙箱与宿主接口

- 新增 `luaL_openlbslibs`。
- 新增 `lbindhost.h`。
- 接入宿主 resolver/writer。

## 9. 工作包 B 验收标准

工作包 B 完成后，应明确：

- XML 如何把 Lua 源码交给 LBS 编译器。
- `bind` / `var bind` / `var <-` 如何进入 parser。
- 绑定声明如何从 Lua 代码中导出为 metadata。
- 绑定变量如何参与依赖图。
- 完整 Lua 脚本与响应式绑定区域如何共存。
- 第一阶段为什么不改 Lua VM 主循环。
