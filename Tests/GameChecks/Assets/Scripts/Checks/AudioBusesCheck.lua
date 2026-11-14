-- Buses, volumes, sounds played once and the attenuation of a source, from Lua, on the sounds of Assets/Sounds
-- (Tools/make_sounds.py) and the project's default buses: the buses and their volumes, the source's settings, sounds
-- played once counted as they start and end, one too far to be heard, and a source's stopped with it. On the device
-- or the null driver alike.
local Check = require("Scripts/Checks/Check")

local AudioBusesCheck = {}
local oCheck = Check.New("AudioBuses")
local t = {}
local nFrame = 0
local nStep = 1
local nStepFrames = 0

-- A step that waits gives up after this many frames
local kDeadline = 2000

local function Fails(fCall)
	local bOk, sError = pcall(fCall)
	return not bOk, tostring(sError)
end

local function Mentions(sText, sPart)
	return type(sText) == "string" and sText:find(sPart, 1, true) ~= nil
end

local function Near(nValue, nExpected)
	return type(nValue) == "number" and math.abs(nValue - nExpected) < 0.001
end

local tSteps = {
	function()
		local nMajor, nMinor = Application.GetApiVersion()
		oCheck:Expect(nMajor == 1 and nMinor >= 23, "the Lua API is 1.23 or a later 1.x: " .. nMajor .. "." .. nMinor)

		-- The project's buses: the defaults, as the checks' project names none
		local tBuses = Audio.GetBuses()
		oCheck:Equal(table.concat(tBuses, ","), "Music,Effects,Interface,Ambience", "the default buses")
		oCheck:Expect(Near(Audio.GetVolume("Master"), 1.0) and Near(Audio.GetVolume("Effects"), 1.0), "every volume starts at 1")

		Audio.SetVolume("Effects", 0.4)
		Audio.SetVolume("Master", 0.8)
		oCheck:Expect(Near(Audio.GetVolume("Effects"), 0.4), "a bus's volume")
		oCheck:Expect(Near(Audio.GetVolume("Master"), 0.8), "the Master's volume")
		oCheck:Expect(Near(Audio.GetVolume("Music"), 1.0), "and the other buses keep theirs")

		local bFailed, sError = Fails(function() Audio.SetVolume("Nope", 1) end)
		oCheck:Expect(bFailed and Mentions(sError, "no bus 'Nope'"), "a bus the project lacks: " .. sError)
		bFailed, sError = Fails(function() Audio.SetVolume("Music", -1) end)
		oCheck:Expect(bFailed and Mentions(sError, "0 or more"), "a volume below 0: " .. sError)
		bFailed = Fails(function() Audio.GetVolume("Nope") end)
		oCheck:Expect(bFailed, "the volume of a bus the project lacks")

		-- A source's settings, from their defaults
		local oSource = t.oSpeaker
		oCheck:Equal(oSource:GetBus(), "Master", "no bus at first")
		oCheck:Equal(oSource:GetAttenuationModel(), AttenuationModel.EXPONENTIAL_DISTANCE, "exponential at first")
		oCheck:Expect(Near(oSource:GetRolloff(), 1.0) and Near(oSource:GetMaxDistance(), 500.0) and Near(oSource:GetPitchVariation(), 0.0), "rolloff 1, 500 metres, no variation")

		oSource:SetBus("Ambience")
		oSource:SetAttenuationModel(AttenuationModel.LINEAR_DISTANCE)
		oSource:SetRolloff(0.5)
		oSource:SetMaxDistance(80)
		oSource:SetPitchVariation(0.1)
		oCheck:Equal(oSource:GetBus(), "Ambience", "a bus set")
		oCheck:Equal(oSource:GetAttenuationModel(), AttenuationModel.LINEAR_DISTANCE, "a model set")
		oCheck:Expect(Near(oSource:GetRolloff(), 0.5) and Near(oSource:GetMaxDistance(), 80.0) and Near(oSource:GetPitchVariation(), 0.1), "its numbers set")
		bFailed, sError = Fails(function() oSource:SetBus("Nope") end)
		oCheck:Expect(bFailed and Mentions(sError, "no bus 'Nope'"), "a source's bus the project lacks: " .. sError)
		oCheck:Equal(oSource:GetBus(), "Ambience", "which leaves its bus")
		oSource:SetBus("Master")
		oCheck:Equal(oSource:GetBus(), "Master", "back to none")

		-- Sounds played once: refused arguments
		bFailed, sError = Fails(function() Audio.PlayOneShot(nil) end)
		oCheck:Expect(bFailed and Mentions(sError, "a sound is needed"), "no sound: " .. sError)
		bFailed, sError = Fails(function() Audio.PlayOneShot(t.oClick, { volum = 1 }) end)
		oCheck:Expect(bFailed and Mentions(sError, "unknown option 'volum'"), "an unknown option: " .. sError)
		bFailed, sError = Fails(function() Audio.PlayOneShot(t.oClick, { position = 3 }) end)
		oCheck:Expect(bFailed and Mentions(sError, "'position' must be a Vector3"), "an option of the wrong kind: " .. sError)
		bFailed, sError = Fails(function() Audio.PlayOneShot(t.oClick, { bus = "Nope" }) end)
		oCheck:Expect(bFailed and Mentions(sError, "no bus 'Nope'"), "a sound played once on a bus the project lacks: " .. sError)
		oCheck:Equal(Audio.GetVoiceCount(), 0, "none of them played")

		-- Three at once, side by side
		local bClick = Audio.PlayOneShot(t.oClick, { bus = "Effects", pitchVariation = 0.2 })
		local bTone = Audio.PlayOneShot(t.oTone, { volume = 0.5, bus = "Interface" })
		local bNear = Audio.PlayOneShot(t.oTone, { position = Vector3.new(3, 0, 4), attenuation = AttenuationModel.INVERSE_DISTANCE })
		oCheck:Expect(bClick and bTone and bNear, "three sounds played once")
		oCheck:Equal(Audio.GetVoiceCount(), 3, "on three voices")

		-- Too far to be heard: it ends at once
		local bFar = Audio.PlayOneShot(t.oTone, { position = Vector3.new(0, 0, 200), attenuation = AttenuationModel.LINEAR_DISTANCE, maxDistance = 100 })
		oCheck:Expect(not bFar, "one too far to be heard ends at once")
		oCheck:Equal(Audio.GetVoiceCount(), 3, "and takes no voice")
		return true
	end,

	-- They end by themselves
	function()
		if Audio.GetVoiceCount() > 0 then
			return false
		end
		oCheck:Expect(true, "the sounds played once ended by themselves")

		-- A source's, beside its own sound, stopped with it
		t.oSpeaker:Play()
		t.oSpeaker:PlayOneShot(t.oTone)
		t.oSpeaker:PlayOneShot(t.oClick, 0.5)
		oCheck:Equal(Audio.GetVoiceCount(), 3, "a source's own sound and two played once")
		t.oSpeaker:Stop()
		oCheck:Equal(Audio.GetVoiceCount(), 0, "all stopped with the source")
		return true
	end,
}

function AudioBusesCheck:OnAwake()
	local oScene = Scenes.GetCurrentScene()
	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:AddCamera()
	oCamera:AddAudioListener()

	t.oTone = Resources.GetSound("Sounds/Tone.wav")
	t.oClick = Resources.GetSound("Sounds/Click.wav")

	local oSpeaker = oScene:CreateActor("Speaker", "")
	oSpeaker:GetTransform():SetLocalPosition(Vector3.new(0, 0, 2))
	t.oSpeaker = oSpeaker:AddAudioSource()
	t.oSpeaker:SetSound(Resources.GetSound("Sounds/Loop.wav"))
	t.oSpeaker:SetLooped(true)
	t.oSpeaker:SetSpatial(true)
end

function AudioBusesCheck:OnUpdate()
	nFrame = nFrame + 1
	if nFrame < 3 or nStep > #tSteps then
		return
	end

	nStepFrames = nStepFrames + 1
	if tSteps[nStep]() then
		nStep = nStep + 1
		nStepFrames = 0
	elseif nStepFrames > kDeadline then
		oCheck:Expect(false, "step " .. nStep .. " still waits on the sounds after " .. kDeadline .. " frames")
		nStep = #tSteps + 1
	end

	if nStep > #tSteps then
		-- Every volume back to 1, so that another game (in the editor) starts the same
		for _, sBus in ipairs(Audio.GetBuses()) do
			Audio.SetVolume(sBus, 1)
		end
		Audio.SetVolume("Master", 1)
		oCheck:Finish()
	end
end

return AudioBusesCheck
