-- The global Update and LateUpdate events: their place in the frame, their step, their owner, their removal
local Check = require("Scripts/Checks/Check")
local Log = require("Scripts/Checks/EventsLog")

local EventsCheck = {}
local oCheck = Check.New("Events")
local nFrame = 0
local oOwner = nil
local nOwnedCalls, nRemovedCalls = 0, 0
local tAddedAt = {}

function EventsCheck:OnStart()
	Events.AddUpdate(function(nDelta)
		Log.tFrames[#Log.tFrames + 1] = "U"
		Log.tSteps[#Log.tSteps + 1] = tostring(nDelta)
	end)

	Events.AddLateUpdate(function()
		local nLast = #Log.tFrames
		Log.tFrames[nLast] = Log.tFrames[nLast] .. "Z"
	end)

	-- Ends with its owner
	oOwner = Scenes.GetCurrentScene():CreateActor("Owner", "")
	Events.AddUpdate(function() nOwnedCalls = nOwnedCalls + 1 end, oOwner)

	-- Removed before it ever runs
	local nRemoved = Events.AddUpdate(function() nRemovedCalls = nRemovedCalls + 1 end)
	oCheck:Expect(Events.Remove(nRemoved), "Remove finds a callback")
	oCheck:Expect(not Events.Remove(nRemoved), "Remove finds it once")

	-- One added while the event runs waits for the next frame
	local nAdder
	nAdder = Events.AddUpdate(function()
		tAddedAt.adder = #Log.tFrames
		Events.Remove(nAdder)
		Events.AddUpdate(function()
			tAddedAt.added = tAddedAt.added or #Log.tFrames
		end)
	end)

	local bOk, sError = pcall(Events.AddUpdate, 42)
	oCheck:Expect(not bOk and tostring(sError):find("a function is expected") ~= nil, "a callback must be a function")
end

function EventsCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame == 2 then
		oOwner:Destroy()
	elseif nFrame == 6 then
		-- The frames written so far are complete; this one is still running
		for nIndex = 1, #Log.tFrames - 1 do
			oCheck:Equal(Log.tFrames[nIndex], "UBLZ", "order in frame " .. nIndex)
			local sGlobal, sBehaviour = Log.tSteps[nIndex]:match("^([^/]+)/(.+)$")
			oCheck:Equal(sGlobal, sBehaviour, "step in frame " .. nIndex)
		end
		oCheck:Expect(#Log.tFrames >= 5, "frames recorded")
		oCheck:Equal(nOwnedCalls, 2, "calls before the owner was destroyed")
		oCheck:Equal(nRemovedCalls, 0, "calls of a removed callback")
		oCheck:Equal(tAddedAt.adder, 1, "frame of the callback that adds another")
		oCheck:Equal(tAddedAt.added, 2, "first frame of the callback added while the event ran")
		oCheck:Finish()
	end
end

return EventsCheck
