-- Sim shim module - provides normalized interface to native sim extension
local native = rawget(_G, "sim") or error("native sim not loaded")

local M = {}

function M.register_listener(u) 
    return native.register_listener(u) 
end

function M.read_snapshot() 
    return native.read_snapshot() 
end

function M.read_snapshots()
    if native.read_snapshots then
        local prev, curr, alpha, tick = native.read_snapshots()
        return prev, curr, alpha or 0, tick or (native.get_tick and native.get_tick() or 0)
    else
        local curr = native.read_snapshot and native.read_snapshot() or nil
        return nil, curr, 0, (native.get_tick and native.get_tick() or 0)
    end
end

-- Forward other functions
function M.get_tick() return native.get_tick() end
function M.get_dt() return native.get_dt() end
function M.set_rate(hz) return native.set_rate(hz) end
function M.pause(paused) return native.pause(paused) end
function M.step_n(n) return native.step_n(n) end
function M.get_stats() return native.get_stats() end

return setmetatable(M, { __index = native })
