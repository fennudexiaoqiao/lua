cpp_log("Lua battle rules loaded")

function simulate_battle_round(attacker, defender)
  cpp_log(string.format("%s attacks %s", attacker.name, defender.name))

  local raw_damage = attacker.attack - math.floor(defender.hp * 0.05)
  local clamped_damage = math.max(raw_damage, 1)
  local final_damage = cpp_scale_damage(clamped_damage, 1.5)
  local remaining_hp = math.max(defender.hp - final_damage, 0)

  local summary = string.format(
    "%s deals %d damage, %s has %d HP left",
    attacker.name,
    final_damage,
    defender.name,
    remaining_hp
  )

  return final_damage, summary
end