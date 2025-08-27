-- Simulation Bus: centralizes sim messages and snapshot distribution

local M = { 
	subscribers = {}, 
	subscribers_world = {},
	subscribers_visual = {},
	sim = nil,
	-- Store snapshot data for managers to access
	current_snapshots = {
		prev = nil,
		curr = nil,
		alpha = 0,
		tick = 0
	},
	debug_ticks_printed = 0
}

function M.on_init(sim)
	M.sim = sim
	print("Sim bus initialized with sim API")
end

function M.subscribe(subscriber_url)
	if not subscriber_url then return end
	table.insert(M.subscribers, subscriber_url)
end

function M.subscribe_world(subscriber_url)
	if not subscriber_url then return end
	table.insert(M.subscribers_world, subscriber_url)
end

function M.subscribe_visual(subscriber_url)
	if not subscriber_url then return end
	table.insert(M.subscribers_visual, subscriber_url)
end

local function remove_from_list(list, url)
	for i = #list, 1, -1 do
		if list[i] == url then
			table.remove(list, i)
			break
		end
	end
end

function M.unsubscribe(subscriber_url)
	if not subscriber_url then return end
	remove_from_list(M.subscribers, subscriber_url)
	remove_from_list(M.subscribers_world, subscriber_url)
	remove_from_list(M.subscribers_visual, subscriber_url)
end

-- Method for managers to get the current snapshot data
function M.get_snapshots()
	-- Read fresh snapshots each time to avoid invalid buffer references
	if not M.sim then 
		return nil, nil, 0, 0
	end
	local prev, curr, alpha, tick = M.sim.read_snapshots()
	
	-- Debug: log what we received

	
	return prev, curr, alpha, tick
end

function M.on_tick()
	if not M.sim then 
		print("Sim bus: no sim API available")
		return 
	end
	
	-- Read snapshots fresh for debug info only
	local prev, curr, alpha, tick = M.sim.read_snapshots()
	-- Minimal debug output for first few ticks
	if M.debug_ticks_printed < 3 then
		local entity_count = 0
		if curr then
			local data_stream = buffer.get_stream(curr, hash("data"))
			entity_count = data_stream and (#data_stream / 8) or 0
		end
		print("BUS: tick=" .. tick .. " entities=" .. entity_count)
		M.debug_ticks_printed = M.debug_ticks_printed + 1
	end
	
	-- Phase 1: world subscribers (compute visibility/chunks first)
	for i = 1, #M.subscribers_world do
		msg.post(M.subscribers_world[i], "bus_tick")
	end
	-- Phase 2: visual subscribers (render after world)
	for i = 1, #M.subscribers_visual do
		msg.post(M.subscribers_visual[i], "bus_tick")
	end
	-- Fallback/others
	for i = 1, #M.subscribers do
		msg.post(M.subscribers[i], "bus_tick")
	end
end

local function broadcast(list, message_id, message)
	for i = 1, #list do
		msg.post(list[i], message_id, message)
	end
end

function M.on_spawn(message)
	-- World first, then visual, then generic
	broadcast(M.subscribers_world, "bus_spawn", message)
	broadcast(M.subscribers_visual, "bus_spawn", message)
	broadcast(M.subscribers, "bus_spawn", message)
end

function M.on_despawn(message)
	broadcast(M.subscribers_world, "bus_despawn", message)
	broadcast(M.subscribers_visual, "bus_despawn", message)
	broadcast(M.subscribers, "bus_despawn", message)
end

function M.on_event(message)
	broadcast(M.subscribers_world, "bus_event", message)
	broadcast(M.subscribers_visual, "bus_event", message)
	broadcast(M.subscribers, "bus_event", message)
end

return M


