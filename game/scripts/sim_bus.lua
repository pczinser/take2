-- Simulation Bus: centralizes sim messages and snapshot distribution

local M = { 
    subscribers = {}, 
    sim = nil,
    -- Store snapshot data for managers to access
    current_snapshots = {
        prev = nil,
        curr = nil,
        alpha = 0,
        tick = 0
    }
}

function M.on_init(sim)
	M.sim = sim
	print("Sim bus initialized with sim API")
end

function M.subscribe(subscriber_url)
	if not subscriber_url then return end
	table.insert(M.subscribers, subscriber_url)
	print("Sim bus: subscriber added, total subscribers:", #M.subscribers)
end

function M.unsubscribe(subscriber_url)
	if not subscriber_url then return end
	for i = #M.subscribers, 1, -1 do
		if M.subscribers[i] == subscriber_url then
			table.remove(M.subscribers, i)
			break
		end
	end
end

-- Method for managers to get the current snapshot data
function M.get_snapshots()
	return M.current_snapshots.prev, M.current_snapshots.curr, M.current_snapshots.alpha, M.current_snapshots.tick
end

function M.on_tick()
	print("Sim bus: on_tick called")
	if not M.sim then 
		print("Sim bus: no sim API available")
		return 
	end
	
	-- Read snapshots once and store them
	local prev, curr, alpha, tick = M.sim.read_snapshots()
	M.current_snapshots.prev = prev
	M.current_snapshots.curr = curr
	M.current_snapshots.alpha = alpha
	M.current_snapshots.tick = tick
	
	-- Send notification messages to all subscribers (no data, just notification)
	print("Sim bus: sending tick notifications to", #M.subscribers, "subscribers")
	for i = 1, #M.subscribers do
		local subscriber_url = M.subscribers[i]
		msg.post(subscriber_url, "bus_tick")
	end
end

function M.on_spawn(message)
	-- Send spawn messages to all subscribers
	for i = 1, #M.subscribers do
		local subscriber_url = M.subscribers[i]
		msg.post(subscriber_url, "bus_spawn", message)
	end
end

function M.on_despawn(message)
	-- Send despawn messages to all subscribers
	for i = 1, #M.subscribers do
		local subscriber_url = M.subscribers[i]
		msg.post(subscriber_url, "bus_despawn", message)
	end
end

function M.on_event(message)
	-- Send event messages to all subscribers
	for i = 1, #M.subscribers do
		local subscriber_url = M.subscribers[i]
		msg.post(subscriber_url, "bus_event", message)
	end
end

return M


