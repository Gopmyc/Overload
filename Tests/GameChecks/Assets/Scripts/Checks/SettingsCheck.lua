-- The project file's settings reach the game: layers, Json and storage limits, the longest frame step
local Check = require("Scripts/Checks/Check")

local SettingsCheck = {}
local oCheck = Check.New("Settings")
local nFrame = 0

local function Nest(nDepth)
	local tValue = {}
	for _ = 2, nDepth do
		tValue = { tValue }
	end
	return tValue
end

function SettingsCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame == 2 then
		oCheck:Expect(string.match(Application.GetEngineVersion(), "^%d+%.%d+%.%d+") ~= nil, "engine version")

		oCheck:Equal(Physics.GetLayerIndex("Enemy"), 2, "named layer")
		oCheck:Equal(Physics.GetLayerName(3), "Floor", "layer name")
		oCheck:Equal(Physics.GetLayerIndex("Nope"), nil, "unknown layer")
		oCheck:Equal(Physics.DoLayersCollide(1, 2), false, "ignored pair")
		oCheck:Equal(Physics.DoLayersCollide(2, 1), false, "ignored pair, either way")
		oCheck:Equal(Physics.DoLayersCollide(1, 3), true, "other pairs collide")

		oCheck:Expect(Json.Encode(Nest(8)) ~= nil, "Json within json_max_depth")
		local sText, sError = Json.Encode(Nest(9))
		oCheck:Expect(sText == nil and sError ~= nil, "Json past json_max_depth")

		local bWritten, sWriteError = Storage.Write("big.json", string.rep("x", 1024 * 1024 + 1))
		oCheck:Equal(sWriteError, "file too large", "storage_max_file_mib")
		oCheck:Expect(not bWritten, "a file past the limit is not written")
		oCheck:Expect(Storage.Write("small.json", "{}") == true, "a small file is written")
		oCheck:Expect(Storage.Delete("small.json") == true, "and deleted")
	elseif nFrame == 3 then
		-- A long frame: the next step is capped by max_frame_delta
		local nSum = 0
		for nIndex = 1, 30000000 do
			nSum = nSum + nIndex % 7
		end
		oCheck:Expect(nSum > 0, "busy frame")
	elseif nFrame == 4 then
		oCheck:Near(nDelta, 0.05, "step after a long frame, capped by max_frame_delta", 0.0001)
		oCheck:Finish()
	end
end

return SettingsCheck
