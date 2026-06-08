# C++/Lua 双向绑定 Demo — 时序图

## 1. 初始化阶段

```mermaid
sequenceDiagram
    participant Main as main()
    participant Host as BindingHost
    participant Lua as Lua State
    participant Script as demo.lua

    Main->>Host: add_property("extern.plc.motor.speed", INT, 0)
    Main->>Host: add_property("extern.plc.motor.temp", REAL, 25.0)
    Main->>Host: add_property("extern.plc.motor.running", BOOL, false)
    Main->>Host: add_property("extern.plc.motor.name", STRING, "MOTOR-01")
    Main->>Host: add_property("ui.slider.value", INT, 0)
    Main->>Host: add_property("ui.speedLabel.text", STRING, "0 rpm")
    Main->>Host: add_property("ui.statusLed.on", BOOL, false)
    Main->>Host: add_property("state.pageName", STRING, "MotorPanel")

    Main->>Lua: push_proxy_table("extern")
    Main->>Lua: push_proxy_table("ui")
    Main->>Lua: push_proxy_table("state")
    Main->>Lua: register(lbs_bind, lbs_bind2way, cpp_set, cpp_print)

    Main->>Script: luaL_dofile("demo.lua")
```

## 2. 读取属性（Lua → C++）

```mermaid
sequenceDiagram
    participant Script as demo.lua
    participant Proxy as extern (metatable)
    participant Host as BindingHost

    Script->>Proxy: extern.plc.motor.speed
    Proxy->>Proxy: __index(extern, "plc") → 返回子代理
    Note over Proxy: 子代理 _path="extern.plc"
    Proxy->>Proxy: __index(子代理, "motor") → 返回子代理
    Note over Proxy: 子代理 _path="extern.plc.motor"
    Proxy->>Proxy: __index(子代理, "speed") → 找到属性
    Proxy->>Host: find_property("extern.plc.motor.speed")
    Host-->>Proxy: Property { type=INT, value=0 }
    Proxy-->>Script: 0

    Script->>Proxy: state.pageName
    Proxy->>Host: find_property("state.pageName")
    Host-->>Proxy: Property { type=STRING, value="MotorPanel" }
    Proxy-->>Script: "MotorPanel"
```

## 3. 单向绑定（lbs_bind）

```mermaid
sequenceDiagram
    participant Script as demo.lua
    participant C as l_bind()
    participant Host as BindingHost

    Script->>C: lbs_bind("ui.speedLabel.text", "extern.plc.motor.speed")
    C->>Host: add_listener("extern.plc.motor.speed", callback)
    Note over Host: callback: set("ui.speedLabel.text", v)
    C->>Host: get("extern.plc.motor.speed") → 0
    C->>Host: set("ui.speedLabel.text", 0, notify=true)
    Note over Host: ui.speedLabel.text = 0 (初始同步)
    C-->>Script: [bind] ui.speedLabel.text <- extern.plc.motor.speed
```

## 4. 双向绑定（lbs_bind2way）+ 用户操作

```mermaid
sequenceDiagram
    participant Script as demo.lua
    participant C as l_bind2way()
    participant Host as BindingHost

    Script->>C: lbs_bind2way("ui.slider.value", "extern.plc.motor.speed")
    C->>Host: add_listener("ui.slider.value", callback)
    Note over Host: callback: set("extern.plc.motor.speed", v)
    C->>Host: add_listener("extern.plc.motor.speed", callback)
    Note over Host: callback: set("ui.slider.value", v)
    C->>Host: set("extern.plc.motor.speed", get("ui.slider.value"))
    Note over Host: 初始同步: motor.speed ← slider.value (0→0, 跳过)
    C-->>Script: [bind2way] ui.slider.value <=> extern.plc.motor.speed

    Note over Script: --- 用户拖拽滑块到 75 ---
    Script->>Host: ui.slider.value = 75
    Note over Host: __newindex → set("ui.slider.value", 75)
    Host->>Host: value: 0 → 75, 通知 listeners
    Host->>Host: listener: set("extern.plc.motor.speed", 75)
    Note over Host: motor.speed: 0 → 75, 通知 listeners
    Host->>Host: listener1 (lbs_bind): set("ui.speedLabel.text", 75)
    Note over Host: speedLabel.text: "0 rpm" → 75
    Host->>Host: listener2 (bind2way): set("ui.slider.value", 75)
    Note over Host: slider.value 已是 75, value == v, 跳过 ← 断开循环
```

## 5. PLC 值变化（C++ → Lua）

```mermaid
sequenceDiagram
    participant Script as demo.lua
    participant C as cpp_set()
    participant Host as BindingHost

    Script->>C: cpp_set("extern.plc.motor.speed", 100)
    C->>Host: set("extern.plc.motor.speed", 100, notify=true)
    Note over Host: motor.speed: 75 → 100, 通知 listeners
    Host->>Host: listener1 (lbs_bind): set("ui.speedLabel.text", 100)
    Note over Host: speedLabel.text → 100
    Host->>Host: listener2 (bind2way): set("ui.slider.value", 100)
    Note over Host: slider.value: 75 → 100, 通知 listeners
    Host->>Host: bind2way反向: set("extern.plc.motor.speed", 100)
    Note over Host: motor.speed 已是 100, value == v, 跳过 ← 断开循环

    Script->>C: cpp_set("extern.plc.motor.speed", 0)
    Note over Host: 同上流程, 所有值归零
```

## 6. 布尔类型绑定

```mermaid
sequenceDiagram
    participant Script as demo.lua
    participant C as cpp_set()
    participant Host as BindingHost

    Script->>C: lbs_bind("ui.statusLed.on", "extern.plc.motor.running")
    Note over Host: listener: set("ui.statusLed.on", v)

    Script->>C: cpp_set("extern.plc.motor.running", true)
    C->>Host: set("extern.plc.motor.running", true)
    Host->>Host: running: false → true
    Host->>Host: listener: set("ui.statusLed.on", true)
    Note over Host: statusLed.on: false → true

    Script->>C: cpp_set("extern.plc.motor.running", false)
    C->>Host: set("extern.plc.motor.running", false)
    Host->>Host: running: true → false
    Host->>Host: listener: set("ui.statusLed.on", false)
    Note over Host: statusLed.on: true → false
```

## 7. 条件绑定（lbs_bind_if）— 仅当 guard 为真时生效

```mermaid
sequenceDiagram
    participant Script as demo.lua
    participant C as l_bind_if()
    participant Host as BindingHost

    Script->>C: lbs_bind_if("ui.tempLabel.text", "extern.plc.motor.temp", "extern.plc.motor.running")
    C->>Host: add_listener("extern.plc.motor.temp", callback_src)
    Note over Host: callback_src: if running → set(tempLabel, temp)
    C->>Host: add_listener("extern.plc.motor.running", callback_cond)
    Note over Host: callback_cond: if running → set(tempLabel, temp)
    C->>Host: get("extern.plc.motor.running") → false
    Note over C: 初始条件为 false, 不同步
    C-->>Script: [bind_if] tempLabel <- temp when running

    Note over Script: --- Motor STOPPED: temp 变化被阻断 ---
    Script->>Host: cpp_set("extern.plc.motor.temp", 80.5)
    Host->>Host: temp: 25.0 → 80.5, 通知 listener
    Host->>Host: callback_src: iec_truthy(running) → false → 跳过
    Note over Host: tempLabel 不变

    Note over Script: --- Motor STARTED: 立即同步 ---
    Script->>Host: cpp_set("extern.plc.motor.running", true)
    Host->>Host: running: false → true, 通知 listener
    Host->>Host: callback_cond: iec_truthy(running) → true
    Host->>Host: set("ui.tempLabel.text", get("extern.plc.motor.temp"))
    Note over Host: tempLabel = 80.5

    Note over Script: --- Motor RUNNING: temp 变化正常传播 ---
    Script->>Host: cpp_set("extern.plc.motor.temp", 92.3)
    Host->>Host: temp: 80.5 → 92.3, 通知 listener
    Host->>Host: callback_src: iec_truthy(running) → true
    Host->>Host: set("ui.tempLabel.text", 92.3)

    Note over Script: --- Motor STOPPED: temp 变化再次阻断 ---
    Script->>Host: cpp_set("extern.plc.motor.running", false)
    Host->>Host: running: true → false, 通知 listener
    Host->>Host: callback_cond: iec_truthy(running) → false → 跳过
    Script->>Host: cpp_set("extern.plc.motor.temp", 45.0)
    Host->>Host: callback_src: iec_truthy(running) → false → 跳过
    Note over Host: tempLabel 保持 92.3 (最后有效值)
```

## 8. LBS 语法中的条件绑定

```lua
-- 条件单向绑定 (LBS 扩展语法)
bind ui.tempLabel.text <- extern.plc.motor.temp when extern.plc.motor.running

-- 等价 Demo API
lbs_bind_if("ui.tempLabel.text", "extern.plc.motor.temp", "extern.plc.motor.running")
```

`LBS_BindDecl` 新增字段:

| 字段 | 类型 | 说明 |
|------|------|------|
| `condition` | `LBS_BindPath *` | 守卫路径，NULL 表示无条件 |

## 9. Once 绑定（lbs_bind_once）— Vue v-once 语义

```mermaid
sequenceDiagram
    participant Script as demo.lua
    participant C as l_bind_once()
    participant Host as BindingHost

    Script->>C: lbs_bind_once("ui.initValue.text", "extern.plc.motor.speed")
    C->>Host: get("extern.plc.motor.speed") → 45
    C->>Host: set("ui.initValue.text", 45, notify=true)
    Note over Host: initValue = 45 (snapshot taken)
    Note over C: ⚠ No listener registered
    C-->>Script: [bind_once] initValue <- speed (one-shot)

    Note over Script: --- Speed changes: once binding ignores ---
    Script->>Host: cpp_set("extern.plc.motor.speed", 88)
    Host->>Host: speed: 45 → 88, 通知 listeners
    Note over Host: initValue 没有 listener → 不响应
    Note over Host: initValue 保持 45

    Script->>Host: cpp_set("extern.plc.motor.speed", 12)
    Note over Host: initValue 仍保持 45
```

## 10. 四种绑定模式对比

| 模式 | LBS 语法 | Demo API | 更新时机 |
|------|---------|----------|---------|
| 单向 | `bind A <- B` | `lbs_bind` | B 每次变化 → A |
| 双向 | `bind A <=> B` | `lbs_bind2way` | 任一侧变化 → 另一侧 |
| 条件 | `bind A <- B when C` | `lbs_bind_if` | B 变化 + C 为真 → A |
| 一次 | `bind A <- B once` | `lbs_bind_once` | **仅绑定时一次** → A |

```mermaid
graph LR
    subgraph "单向 bind"
        S1[source] -->|每次变化| T1[target]
    end

    subgraph "双向 bind2way"
        S2[source] <-->|每次变化| T2[target]
    end

    subgraph "条件 bind_if"
        S3[source] -->|guard 为真时| T3[target]
        G3[guard] -.->|控制| S3
    end

    subgraph "一次 bind_once"
        S4[source] -->|仅绑定时| T4[target]
        S4 -.->|后续变化 ✗| T4
    end
```

## 11. 整体依赖图

```mermaid
graph TD
    subgraph "C++ BindingHost"
        P1["extern.plc.motor.speed<br/>(INT)"]
        P2["extern.plc.motor.temp<br/>(REAL)"]
        P3["extern.plc.motor.running<br/>(BOOL)"]
        P4["extern.plc.motor.name<br/>(STRING)"]
        U1["ui.slider.value<br/>(INT)"]
        U2["ui.speedLabel.text<br/>(STRING)"]
        U3["ui.statusLed.on<br/>(BOOL)"]
        U4["ui.tempLabel.text<br/>(STRING)"]
        U5["ui.initValue.text<br/>(STRING)"]
        S1["state.pageName<br/>(STRING)"]
    end

    subgraph "Lua Proxy"
        EXT["extern"]
        UI["ui"]
        ST["state"]
    end

    EXT -.->|__index / __newindex| P1
    EXT -.->|__index / __newindex| P2
    EXT -.->|__index / __newindex| P3
    EXT -.->|__index / __newindex| P4
    UI  -.->|__index / __newindex| U1
    UI  -.->|__index / __newindex| U2
    UI  -.->|__index / __newindex| U3
    ST  -.->|__index / __newindex| S1

    P1 -->|lbs_bind| U2
    P1 <-->|lbs_bind2way| U1
    P3 -->|lbs_bind| U3
    P2 -->|lbs_bind_if<br/>when P3| U4["ui.tempLabel.text<br/>(STRING)"]
    P1 -->|lbs_bind_once<br/>(one-shot)| U5

    style P1 fill:#f96,stroke:#333
    style U1 fill:#f96,stroke:#333
    style U2 fill:#9cf,stroke:#333
    style U3 fill:#9f6,stroke:#333
    style P3 fill:#9f6,stroke:#333
    style P2 fill:#ff9,stroke:#333
    style U4 fill:#ff9,stroke:#333
```

| 图例 | 含义 |
|------|------|
| 🟠 橙色 | 双向绑定节点（`<=>`） |
| 🔵 蓝色 | 单向绑定 target（`<-`） |
| 🟢 绿色 | 单向绑定 source → target |
| 🟡 黄色 | 条件绑定（`when` 守卫） |
| ⬜ 灰色 | 一次绑定（`once` 快照） |
