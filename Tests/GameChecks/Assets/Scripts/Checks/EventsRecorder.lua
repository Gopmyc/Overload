-- A Behaviour among the others: it writes where its callbacks fall in the frame, and the step it receives
local Log = require("Scripts/Checks/EventsLog")

local EventsRecorder = {}

function EventsRecorder:OnUpdate(nDelta)
	local nFrame = #Log.tFrames
	if nFrame > 0 then
		Log.tFrames[nFrame] = Log.tFrames[nFrame] .. "B"
		Log.tSteps[nFrame] = Log.tSteps[nFrame] .. "/" .. nDelta
	end
end

function EventsRecorder:OnLateUpdate(nDelta)
	local nFrame = #Log.tFrames
	if nFrame > 0 then
		Log.tFrames[nFrame] = Log.tFrames[nFrame] .. "L"
	end
end

return EventsRecorder
