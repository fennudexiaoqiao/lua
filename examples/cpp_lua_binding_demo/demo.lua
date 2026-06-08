-- demo.lua — C++/Lua bidirectional binding demo
-- Demonstrates: one-way binding, two-way binding, PLC value changes, UI updates

print("[Lua] Binding demo started")
print("[Lua] page: " .. state.pageName)

-- ============================================================
-- 1. Read initial values from C++ host
-- ============================================================
print(string.format("[Lua] Initial motor speed: %d", extern.plc.motor.speed))
print(string.format("[Lua] Initial motor temp:  %.1f", extern.plc.motor.temp))
print(string.format("[Lua] Motor running: %s", tostring(extern.plc.motor.running)))
print(string.format("[Lua] Motor name: %s", extern.plc.motor.name))

-- ============================================================
-- 2. Register ONE-WAY bindings: source changes → target updates
-- ============================================================
lbs_bind("ui.speedLabel.text", "extern.plc.motor.speed")
print(string.format("[Lua] After one-way bind: speedLabel = '%s'", ui.speedLabel.text))

-- format temperature binding: we'll use a listener + manual format
-- (the binding system copies raw values; formatting is done in the listener)

-- ============================================================
-- 3. Register TWO-WAY binding: UI slider ↔ PLC motor speed
-- ============================================================
lbs_bind2way("ui.slider.value", "extern.plc.motor.speed")
print(string.format("[Lua] After two-way bind: slider=%d, motor=%d",
      ui.slider.value, extern.plc.motor.speed))

-- ============================================================
-- 4. Simulate USER dragging the UI slider (Lua writes to ui.*)
--    → propagates to extern.plc.motor.speed via bind2way
--    → propagates to ui.speedLabel.text via bind
-- ============================================================
print("\n--- User drags slider to 75 ---")
ui.slider.value = 75
cpp_print("ui.slider.value")
cpp_print("extern.plc.motor.speed")
cpp_print("ui.speedLabel.text")

print("\n--- User drags slider to 30 ---")
ui.slider.value = 30
cpp_print("ui.slider.value")
cpp_print("extern.plc.motor.speed")
cpp_print("ui.speedLabel.text")

-- ============================================================
-- 5. Simulate PLC value change (C++ writes to extern.*)
--    → propagates to ui.slider.value via bind2way
--    → propagates to ui.speedLabel.text via bind
-- ============================================================
print("\n--- PLC reports speed change to 100 ---")
cpp_set("extern.plc.motor.speed", 100)
cpp_print("extern.plc.motor.speed")
cpp_print("ui.slider.value")
cpp_print("ui.speedLabel.text")

print("\n--- PLC reports speed change to 0 ---")
cpp_set("extern.plc.motor.speed", 0)
cpp_print("extern.plc.motor.speed")
cpp_print("ui.slider.value")
cpp_print("ui.speedLabel.text")

-- ============================================================
-- 6. Other property types
-- ============================================================
print("\n--- Toggle motor running state ---")
print(string.format("Before: running=%s, led=%s",
      tostring(extern.plc.motor.running), tostring(ui.statusLed.on)))

lbs_bind("ui.statusLed.on", "extern.plc.motor.running")
cpp_set("extern.plc.motor.running", true)
print(string.format("After PLC start: running=%s, led=%s",
      tostring(extern.plc.motor.running), tostring(ui.statusLed.on)))

cpp_set("extern.plc.motor.running", false)
print(string.format("After PLC stop:  running=%s, led=%s",
      tostring(extern.plc.motor.running), tostring(ui.statusLed.on)))
