-- Sounds as a game hears them, on the sounds of Assets/Sounds (Tools/make_sounds.py): what they are played on, a sound
-- that ends, a loop that lasts, pause, resume and stop, a source made flat while it plays, and files that give no
-- sound. On a machine without an audio device (a CI runner), the null driver mixes the sounds and discards them, so
-- that they play, end and loop as with one: the check expects the same on both.
local Check = require("Scripts/Checks/Check")

local AudioCheck = {}
local oCheck = Check.New("Audio")
local t = {}
local nFrame = 0
local nStep = 1
local nStepFrames = 0
local nWaited = 0

-- A step that waits gives up after this many frames
local kDeadline = 2000

local function AddSource(oScene, sName, sSound)
	local oSource = oScene:CreateActor(sName, ""):AddAudioSource()
	oSource:SetSound(Resources.GetSound(sSound))
	return oSource
end

-- Each step runs once a frame until it returns true. A step that waits on the sounds fails once its deadline is past
local tSteps = {
	function()
		local nMajor, nMinor = Application.GetApiVersion()
		oCheck:Expect(nMajor == 1 and nMinor >= 22, "the Lua API is 1.22 or a later 1.x: " .. nMajor .. "." .. nMinor)

		t.sDriver = Audio.GetDriver()
		oCheck:Expect(t.sDriver == "device" or t.sDriver == "null", "the sounds are played on a device or mixed: " .. tostring(t.sDriver))
		Debug.Log("Audio check: the sounds are played on the " .. tostring(t.sDriver) .. " driver")

		-- Files that give no sound
		oCheck:Expect(Resources.GetSound("Sounds/Missing.wav") == nil, "a missing file gives no sound")
		oCheck:Expect(Resources.GetSound("Sounds/Broken.wav") == nil, "a file that is not a sound gives none")

		oCheck:Expect(t.oTone:GetSound() ~= nil, "a sound read")
		oCheck:Equal(Audio.GetVoiceCount(), 0, "nothing plays yet")

		t.oTone:Play()
		t.oLoop:Play()
		t.oClick:Play()
		oCheck:Expect(t.oTone:IsPlaying() and t.oLoop:IsPlaying() and t.oClick:IsPlaying(), "three sounds play")
		oCheck:Equal(Audio.GetVoiceCount(), 3, "on three voices")
		return true
	end,

	-- The click, 40 ms long, ends; the tone, 0.5 s long, plays on
	function()
		if t.oClick:IsPlaying() then
			return false
		end
		oCheck:Equal(Audio.GetVoiceCount(), 2, "the click ended, and left its voice")
		oCheck:Expect(t.oTone:IsPlaying(), "the tone plays on")

		t.oLoop:Pause()
		oCheck:Expect(not t.oLoop:IsPlaying(), "the loop paused")
		oCheck:Equal(Audio.GetVoiceCount(), 2, "a paused sound keeps its voice")
		return true
	end,

	-- The tone ends; the paused loop waits
	function()
		if t.oTone:IsPlaying() then
			return false
		end
		oCheck:Equal(Audio.GetVoiceCount(), 1, "the tone ended, the paused loop waits")

		t.oLoop:Resume()
		oCheck:Expect(t.oLoop:IsPlaying(), "the loop resumed")
		nWaited = 0
		return true
	end,

	-- The loop, 0.2 s long, plays past its length: half a second of game time is at least as much mixed
	function()
		nWaited = nWaited + Time.GetUnscaledDeltaTime()
		if nWaited < 0.5 then
			return false
		end
		oCheck:Expect(t.oLoop:IsPlaying(), "the loop plays past its length")
		oCheck:Equal(Audio.GetVoiceCount(), 1, "on its one voice")

		t.oLoop:Stop()
		oCheck:Expect(not t.oLoop:IsPlaying(), "the loop stopped")
		oCheck:Equal(Audio.GetVoiceCount(), 0, "and left its voice")

		-- A sound in the world, made flat while it plays, plays on
		t.oTone:SetSpatial(true)
		t.oTone:Play()
		t.oTone:SetSpatial(false)
		oCheck:Expect(t.oTone:IsPlaying() and not t.oTone:IsSpatial(), "a sound made flat while it plays plays on")
		oCheck:Equal(Audio.GetVoiceCount(), 1, "on one voice")
		return true
	end,

	-- Played again to its end, it ends as it did in the world
	function()
		if t.oTone:IsPlaying() then
			return false
		end
		oCheck:Equal(Audio.GetVoiceCount(), 0, "the tone ended again")
		return true
	end,
}

function AudioCheck:OnAwake()
	local oScene = Scenes.GetCurrentScene()
	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:AddCamera()
	oCamera:AddAudioListener()

	t.oTone = AddSource(oScene, "Tone", "Sounds/Tone.wav")
	t.oLoop = AddSource(oScene, "Loop", "Sounds/Loop.wav")
	t.oLoop:SetLooped(true)
	t.oClick = AddSource(oScene, "Click", "Sounds/Click.wav")
end

function AudioCheck:OnUpdate()
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
		oCheck:Finish()
	end
end

return AudioCheck
