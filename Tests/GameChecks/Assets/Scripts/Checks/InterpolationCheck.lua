-- Two boxes falling side by side, one interpolated (from the scene file) and one not: the fixed update sees the same
-- physics for both; the rest of the frame sees the interpolated one between its last two steps; moving it teleports it
local Check = require("Scripts/Checks/Check")

local InterpolationCheck = {}
local oCheck = Check.New("Interpolation")
local nStep = 0
local tSteps = {}
local nFrames, nOutside, nPlainOff = 0, 0, 0
local nTeleportStep = nil

local function Height(sName)
	return Scenes.GetCurrentScene():FindActorByName(sName):GetTransform():GetWorldPosition().y
end

function InterpolationCheck:OnFixedUpdate(nDelta)
	nStep = nStep + 1

	local oScene = Scenes.GetCurrentScene()
	local oSmooth = oScene:FindActorByName("Smooth")

	if nStep == 1 then
		oCheck:Expect(oSmooth:GetPhysicalObject():IsInterpolated(), "interpolated from the scene file")
		oCheck:Expect(not oScene:FindActorByName("Plain"):GetPhysicalObject():IsInterpolated(), "off when the file says nothing")
	end

	if nTeleportStep == nil then
		-- The fixed update sees the pose of the physics, interpolated or not
		oCheck:Near(Height("Smooth"), Height("Plain"), "same physics at step " .. nStep, 1e-4)
		table.insert(tSteps, Height("Smooth"))
	end

	if nStep == 45 then
		-- Moved from a script, the box starts again from there
		oSmooth:GetTransform():SetPosition(Vector3.new(-2, 30, 0))
		nTeleportStep = nStep
	elseif nTeleportStep ~= nil and nStep == nTeleportStep + 1 then
		-- It keeps its velocity: one step of its fall, some 7 m/s, below where it was put
		oCheck:Near(Height("Smooth"), 30, "teleported where the script put it", 0.2)
	elseif nTeleportStep ~= nil and nStep == nTeleportStep + 10 then
		oCheck:Expect(nFrames > 0, "frames between the steps were seen")
		oCheck:Equal(nOutside, 0, "frames outside the last two steps")
		oCheck:Equal(nPlainOff, 0, "frames where the plain box left its last step")
		oCheck:Finish()
	end
end

function InterpolationCheck:OnUpdate(nDelta)
	if nTeleportStep ~= nil or #tSteps < 2 then return end

	-- On screen, the interpolated box is between its last two steps, the plain one at its last step
	local nPrevious, nLast = tSteps[#tSteps - 1], tSteps[#tSteps]
	local nHeight = Height("Smooth")
	nFrames = nFrames + 1

	if nHeight > math.max(nPrevious, nLast) + 1e-4 or nHeight < math.min(nPrevious, nLast) - 1e-4 then
		nOutside = nOutside + 1
	end

	if math.abs(Height("Plain") - nLast) > 1e-4 then
		nPlainOff = nPlainOff + 1
	end
end

return InterpolationCheck
