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

## 7. 整体依赖图

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

    style P1 fill:#f96,stroke:#333
    style U1 fill:#f96,stroke:#333
    style U2 fill:#9cf,stroke:#333
    style U3 fill:#9f6,stroke:#333
    style P3 fill:#9f6,stroke:#333
```

| 图例 | 含义 |
|------|------|
| 🟠 橙色 | 双向绑定节点（`<=>`） |
| 🔵 蓝色 | 单向绑定 target（`<-`） |
| 🟢 绿色 | 单向绑定 source → target |
