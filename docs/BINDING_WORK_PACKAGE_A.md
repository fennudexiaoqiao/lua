# 工作包 A：XML UI 内嵌 Lua 超集的语言与绑定语义设计

## 1. 目标

本文定义面向 XML UI 的 Lua 超集语言设计。目标不是把 Lua 缩减为表达式求值器，而是在 XML UI 中完整嵌入 Lua，并在 Lua 之上增加 UI 绑定、变量绑定、PLC/外部点位绑定和混合开发语义。

该语言暂称为 Lua Binding Superset，简称 LBS。

## 2. 核心定位

LBS 是 Lua 的超集：

- 合法 Lua 代码在 LBS 中应保持合法。
- XML UI 可以内嵌完整 Lua 脚本块。
- XML UI 属性可以内嵌 Lua 表达式。
- Lua 脚本中可以声明绑定。
- Lua 脚本中可以声明与 UI、状态、PLC 点位关联的变量。
- 绑定语义是语言级一等概念，而不是普通函数调用约定。

这意味着后续实现不能只做“Lua 子集解析器”，而应改造 Lua 前端，使其支持绑定声明、绑定变量声明、依赖提取和可写目标分析。

## 3. XML 与 Lua 的混合开发模型

### 3.1 XML UI 负责结构

XML 负责声明 UI 对象、层级、属性、布局和静态资源。

示例：

```xml
<Window id="mainWindow" title="Motor Panel">
  <Label id="speedLabel" text="${format('%d rpm', motorSpeed)}" />
  <Slider id="speedSlider" value="${motorSpeed}" />

  <lua:script><![CDATA[
    var motorSpeed: INT bind extern.plc.motor.speed mode twoway

    bind ui.speedLabel.text <- format('%d rpm', motorSpeed)
    bind ui.speedSlider.value <=> motorSpeed
  ]]></lua:script>
</Window>
```

### 3.2 Lua 负责行为与绑定

Lua 负责：

- 事件处理。
- 状态计算。
- UI 属性绑定。
- PLC 点位绑定。
- 格式化与转换。
- 局部业务逻辑。

### 3.3 混合开发的三种嵌入形式

LBS 支持三种嵌入形式：

1. XML 属性内联表达式：`${ expression }`
2. XML 子节点脚本块：`<lua:script><![CDATA[ ... ]]></lua:script>`
3. 外部 Lua 文件引用：`<lua:include src="main.lua" />`

三种形式共享同一个绑定上下文和符号表。

## 4. 语言扩展范围

LBS 在完整 Lua 基础上新增以下语言级构造：

- 绑定声明：`bind target <- expression`
- 双向绑定声明：`bind target <=> source`
- 绑定变量声明：`var name: Type bind source [mode oneway|twoway]`
- 类型标注：`name: Type`
- 宿主路径引用：`ui.xxx`、`state.xxx`、`extern.xxx`、`vm.xxx`
- 可选 converter：`converter name`
- 可选 trigger：`trigger change|manual|timer`
- 可选 debounce：`debounce 50ms`

## 5. 绑定声明语义

### 5.1 单向绑定

```lua
bind ui.label.text <- format('%d rpm', motorSpeed)
```

含义：

- 右侧表达式依赖变化时重新计算。
- 计算结果写入左侧目标。
- 左侧必须是可写宿主路径。

### 5.2 双向绑定

```lua
bind ui.slider.value <=> motorSpeed
```

含义：

- `motorSpeed` 变化时更新 `ui.slider.value`。
- `ui.slider.value` 由用户修改时写回 `motorSpeed`。
- 双向绑定要求两侧都可静态解析为可写目标，或显式声明 converter。

### 5.3 绑定声明的扩展参数

```lua
bind ui.input.text <=> userName converter trimString trigger change debounce 80ms
```

含义：

- 使用 `trimString` converter 做读写转换。
- 仅在 UI change 触发时写回。
- 写回前防抖 80ms。

## 6. 变量绑定语义

### 6.1 普通绑定变量

```lua
var motorSpeed: INT bind extern.plc.motor.speed mode twoway
```

含义：

- 声明 Lua 可访问变量 `motorSpeed`。
- 变量值来自外部点位 `extern.plc.motor.speed`。
- 变量支持双向写回。
- `INT` 是宿主强类型，不等同于 Lua number。

### 6.2 UI 属性绑定变量

```lua
var inputText: STRING bind ui.nameEdit.text mode twoway
```

含义：

- `inputText` 与 UI 输入框文本双向同步。
- 脚本读写 `inputText` 时通过绑定系统访问 UI 属性。

### 6.3 派生变量

```lua
var speedText: STRING <- format('%d rpm', motorSpeed)
```

含义：

- `speedText` 是只读派生变量。
- 它依赖 `motorSpeed`。
- 当 `motorSpeed` 变化时重新计算。

## 7. 宿主根对象

LBS 预定义四类宿主根对象：

- `ui`：XML UI 对象树。
- `state`：页面或模块状态。
- `extern`：PLC、设备、远程变量或外部数据点。
- `vm`：ViewModel 或宿主暴露的逻辑对象。

这些根对象不是普通 Lua table，而是宿主代理对象。它们必须支持：

- 静态路径解析。
- 运行时读写。
- 类型查询。
- 权限检查。
- 依赖注册。

## 8. 类型系统

LBS 保留 Lua 动态类型，但对绑定变量和宿主路径引入强类型约束。

第一阶段支持：

- `BOOL`
- `INT`
- `DINT`
- `UINT`
- `REAL`
- `LREAL`
- `STRING`
- `ENUM(name)`
- `ANY`

规则：

- 普通 Lua 局部变量仍按 Lua 语义运行。
- `var name: Type bind ...` 声明的变量按宿主类型校验。
- 写入宿主路径时必须经过类型转换与范围检查。
- PLC 类型优先遵循 IEC 61131 语义。

## 9. 依赖提取规则

LBS 必须支持静态依赖提取。

以下构造产生依赖：

- 绑定表达式右侧读取的宿主路径。
- 绑定变量的 source 路径。
- 派生变量表达式读取的变量或宿主路径。
- XML 属性内联表达式读取的符号。

示例：

```lua
var motorSpeed: INT bind extern.plc.motor.speed
var speedText: STRING <- format('%d rpm', motorSpeed)
bind ui.label.text <- speedText
```

依赖图为：

```text
extern.plc.motor.speed -> motorSpeed -> speedText -> ui.label.text
```

动态路径仍允许在完整 Lua 中使用，但不能作为响应式绑定依赖。若绑定表达式中出现动态路径，编译器必须报诊断，或要求用户显式声明依赖。

## 10. 完整 Lua 与绑定语义的边界

LBS 支持完整 Lua，但不是所有 Lua 代码都自动参与响应式绑定。

### 10.1 可响应式分析区域

以下区域必须可静态分析：

- `bind` 右侧表达式。
- `var ... bind ...` 声明。
- `var ... <- ...` 派生变量声明。
- XML 属性 `${ ... }` 表达式。

### 10.2 普通脚本区域

普通 Lua 函数、事件处理器和流程控制可以完整使用 Lua 能力，但默认不参与自动依赖提取。

示例：

```lua
function onStart()
  print('screen started')
end

function resetSpeed()
  motorSpeed = 0
end
```

上述代码可以读写绑定变量，但其执行时机由事件系统决定，不由响应式调度器自动触发。

## 11. XML 属性内联表达式

XML 属性允许使用 `${ ... }` 嵌入 Lua 表达式：

```xml
<Label text="${speedText}" />
<Label visible="${motorSpeed > 0}" />
```

内联表达式等价于匿名单向绑定：

```lua
bind ui.currentElement.text <- speedText
```

约束：

- 内联表达式必须是单表达式。
- 不允许语句。
- 不允许定义函数。
- 必须支持静态依赖提取。

## 12. 诊断规则

编译器必须报告以下问题：

- 绑定目标不可写。
- 双向绑定不可逆。
- 绑定变量类型不匹配。
- 外部点位不存在。
- UI 路径不存在。
- 动态路径无法静态依赖提取。
- converter 不存在或方向不完整。
- 写回可能丢失精度或越界。

## 13. 安全边界

由于 XML UI 可以完整嵌入 Lua，必须区分开发期与运行期能力。

运行期默认禁用：

- `io`
- `os`
- `debug`
- `package`
- `require`
- 动态加载代码

开发期可通过配置打开部分能力，但发布到目标设备时必须进入沙箱模式。

## 14. 工作包 A 验收标准

工作包 A 完成后，必须明确：

- XML 中 Lua 的三种嵌入形式。
- Lua 超集新增的绑定语法。
- 绑定变量、派生变量、UI 属性绑定的语义。
- 完整 Lua 与响应式绑定区域的边界。
- 静态依赖提取和变量绑定规则。
- 双向绑定和写回限制。
