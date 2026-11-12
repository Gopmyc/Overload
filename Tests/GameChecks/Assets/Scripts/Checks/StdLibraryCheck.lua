-- The standard library in a running game, where only it can be seen: timers and tweens advancing by themselves with
-- no Behaviour to drive them, real time going on while the game is paused, and owners that are actors, one destroyed
-- and one switched off
local Check = require("Scripts/Checks/Check")
local Std = require(":Libraries/Std/Init")

local StdLibraryCheck = {}
local oCheck = Check.New("StdLibrary")
local nFrame = 0
local nPhase = 0
local nGameTime = 0
local t = {}

function StdLibraryCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1
	nGameTime = nGameTime + nDelta
	local oScene = Scenes.GetCurrentScene()

	if nFrame == 2 then
		local oDoomed = oScene:CreateActor("Doomed", "")
		t.oSleeper = oScene:CreateActor("Sleeper", "")

		-- What the doomed actor owns ends with it, though it would still be running
		t.oDoomedTimer = Std.Timers.After(0.2, function() t.bDoomedFired = true end, { owner = oDoomed })
		t.oDoomedTween = Std.Tween.Run({ duration = 5, owner = oDoomed })

		t.oSignal = Std.Signal.New()
		t.tCalls = {}
		local function Listener(sName) return function() t.tCalls[sName] = (t.tCalls[sName] or 0) + 1 end end
		t.oSignal:Connect(Listener("doomed"), oDoomed)
		t.oSignal:Connect(Listener("sleeper"), t.oSleeper)
		t.oSignal:Connect(Listener("behaviour"), self)
		t.oSignal:Connect(Listener("free"))

		t.oSleeper:SetActive(false)
		oDoomed:Destroy()

		-- The game paused: real time goes on, game time waits
		Time.SetScale(0)
		t.oUnscaled = Std.Timers.After(0.05, function() t.bUnscaledFired = true end, { unscaled = true })
		t.oGame = Std.Timers.After(0.05, function() t.bGameFired = true end)
		t.nUnscaledValue = 0
		Std.Tween.Run({ duration = 0.05, unscaled = true, onUpdate = function(nValue) t.nUnscaledValue = nValue end })
		nPhase = 1
	elseif nPhase == 1 and t.bUnscaledFired then
		oCheck:Expect(not t.bGameFired, "a game time timer waits while the game is paused")
		oCheck:Equal(t.nUnscaledValue, 1, "a real time tween ends while the game is paused")
		Time.SetScale(1)
		nGameTime = 0
		nPhase = 2
	elseif nPhase == 2 and t.bGameFired and nGameTime >= 0.4 then
		oCheck:Expect(not t.bDoomedFired and not t.oDoomedTimer:IsPending(), "a timer ends with its owner")
		oCheck:Expect(not t.oDoomedTween:IsRunning(), "a tween ends with its owner")

		t.oSignal:Emit()
		oCheck:Equal(t.tCalls.doomed, nil, "a listener is dropped with its owner")
		oCheck:Equal(t.tCalls.sleeper, nil, "a listener whose owner is switched off is skipped")
		oCheck:Equal(t.tCalls.behaviour, 1, "a Behaviour's table is an owner")
		oCheck:Equal(t.tCalls.free, 1, "a listener without an owner is called")
		oCheck:Equal(t.oSignal:GetCount(), 3, "the dropped listener no longer counts, the skipped one does")

		t.oSleeper:SetActive(true)
		t.oSignal:Emit()
		oCheck:Equal(t.tCalls.sleeper, 1, "a listener whose owner is switched on again is called")
		oCheck:Finish()
	elseif nFrame > 5000 then
		oCheck:Expect(false, "the timers advanced by themselves (phase " .. nPhase .. ")")
		oCheck:Finish()
	end
end

return StdLibraryCheck
