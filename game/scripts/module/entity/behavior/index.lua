-- Behavior registry: requiring this file ensures behavior modules are bundled
local registry = {}

-- Require available behaviors here
local ok_player, player_mod = pcall(require, "game.scripts.module.entity.behavior.player")
if ok_player then registry.player = true end

return registry
