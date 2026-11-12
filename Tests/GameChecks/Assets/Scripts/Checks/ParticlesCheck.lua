-- Particle systems from Lua: the settings and their ranges, a script's burst with its overrides, the rate over the
-- frames, game time stopped while real time goes on, Stop and Clear, refused arguments. It also draws, for a
-- screenshot (--frames 70 --screenshot): additive sparks, alpha smoke, lit cubes as particles, and two systems going
-- round, one in its emitter's space (a ring that follows it) and one in the world's (a trail)
local Check = require("Scripts/Checks/Check")

local ParticlesCheck = {}
local oCheck = Check.New("Particles")
local nFrame = 0
local nPhase = 0
local nGameTime = 0
local nWaited = 0
local t = {}

local function Actor(sName, vPosition)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vPosition)
	return oActor
end

local function Fails(fCall)
	local bOk = pcall(fCall)
	return not bOk
end

local function Drawn()
	local oScene = Scenes.GetCurrentScene()

	local oCamera = Actor("Camera", Vector3.new(0, 2.5, -7))
	oCamera:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(12, 0, 0)))
	oCamera:AddCamera()

	local oSun = oScene:CreateActor("Sun", "")
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(50, -30, 0)))
	oSun:AddDirectionalLight()

	local oAmbient = Actor("Ambient", Vector3.new(0, 2, 3)):AddAmbientSphereLight()
	oAmbient:SetRadius(50)
	oAmbient:SetIntensity(0.4)

	-- Sparks: additive, stretched along their way, falling
	local oSparks = Actor("Sparks", Vector3.new(-3.5, 0, 3)):AddParticleSystem()
	oSparks:SetMaterial(Resources.GetMaterial("Materials/ParticleAdditive.ovmat"))
	oSparks:SetMaxParticles(2000)
	oSparks:SetRate(500)
	oSparks:SetLifetime(0.6, 1.2)
	oSparks:SetSpeed(4, 7)
	oSparks:SetShapeAngle(30)
	oSparks:SetGravity(Vector3.new(0, -9.81, 0))
	oSparks:SetDrag(0.5)
	oSparks:SetSize(0.12, 0.03)
	oSparks:SetColor(Vector4.new(1, 0.75, 0.3, 1), Vector4.new(1, 0.2, 0.05, 0))

	-- Smoke: the engine's material (alpha), rising, growing, turning
	local oSmoke = Actor("Smoke", Vector3.new(0, 0, 5)):AddParticleSystem()
	oSmoke:SetRate(40)
	oSmoke:SetLifetime(2, 3)
	oSmoke:SetSpeed(0.3, 0.8)
	oSmoke:SetShape(ParticleShape.SPHERE)
	oSmoke:SetShapeRadius(0.4)
	oSmoke:SetGravity(Vector3.new(0, 0.6, 0))
	oSmoke:SetSize(0.6, 2.2)
	oSmoke:SetColor(Vector4.new(0.55, 0.55, 0.6, 0.7), Vector4.new(0.35, 0.35, 0.4, 0))
	oSmoke:SetFade(0.2, 0)
	oSmoke:SetSpin(-40, 40)
	oSmoke:SetRandomRotation(true)

	-- Cubes: a model drawn with a lit material whose shader places it as a particle
	local oCubes = Actor("Cubes", Vector3.new(3.5, 0.5, 3)):AddParticleSystem()
	oCubes:SetModel(Resources.GetModel(":Models/Cube.fbx"))
	oCubes:SetMaterial(Resources.GetMaterial("Materials/ParticleMesh.ovmat"))
	oCubes:SetRate(15)
	oCubes:SetLifetime(3)
	oCubes:SetSpeed(1.5, 2.5)
	oCubes:SetShape(ParticleShape.HEMISPHERE)
	oCubes:SetGravity(Vector3.new(0, -1.5, 0))
	oCubes:SetSize(0.25, 0.1)
	oCubes:SetSpin(90, 220)
	oCubes:SetRandomRotation(true)

	-- Two emitters going round: the local system's ring follows its emitter, the world system leaves a trail
	t.oRing = Actor("Ring", Vector3.new(-1.5, 3.2, 4))
	local oRing = t.oRing:AddParticleSystem()
	oRing:SetSpace(ParticleSpace.LOCAL)
	oRing:SetRate(150)
	oRing:SetLifetime(0.8)
	oRing:SetShape(ParticleShape.SPHERE)
	oRing:SetShapeRadius(0.35)
	oRing:SetSpeed(0)
	oRing:SetSize(0.12)
	oRing:SetColor(Vector4.new(0.3, 1, 0.4, 1), Vector4.new(0.3, 1, 0.4, 0))

	t.oTrail = Actor("Trail", Vector3.new(1.5, 3.2, 4))
	local oTrail = t.oTrail:AddParticleSystem()
	oTrail:SetMaterial(Resources.GetMaterial("Materials/ParticleAdditive.ovmat"))
	oTrail:SetRate(300)
	oTrail:SetLifetime(0.8)
	oTrail:SetShape(ParticleShape.POINT)
	oTrail:SetSpeed(0)
	oTrail:SetSize(0.15, 0.02)
	oTrail:SetColor(Vector4.new(0.4, 0.6, 1, 1), Vector4.new(0.4, 0.6, 1, 0))
end

local function CheckApi()
	local oActor = Actor("Probe", Vector3.new(0, -50, 0))
	local oProbe = oActor:AddParticleSystem()
	oCheck:Expect(oActor:GetParticleSystem() == oProbe, "the actor gives its particle system")

	-- Added while the scene plays, it plays at once
	oCheck:Expect(oProbe:IsPlaying(), "a system added while playing plays")
	oProbe:Stop()
	oCheck:Expect(not oProbe:IsPlaying(), "Stop stops it")

	oProbe:SetRate(25)
	oCheck:Equal(oProbe:GetRate(), 25, "rate")
	oProbe:SetLifetime(2, 1)
	local nMin, nMax = oProbe:GetLifetime()
	oCheck:Expect(nMin == 1 and nMax == 2, "a lifetime given backwards is put in order")
	oProbe:SetSpeed(3)
	nMin, nMax = oProbe:GetSpeed()
	oCheck:Expect(nMin == 3 and nMax == 3, "one speed sets both")
	oProbe:SetSize(0.5, 0.25)
	local nStart, nEnd = oProbe:GetSize()
	oCheck:Expect(nStart == 0.5 and nEnd == 0.25, "size over life")
	oProbe:SetColor(Vector4.new(1, 0, 0, 1))
	local vStart, vEnd = oProbe:GetColor()
	oCheck:Expect(vStart.x == 1 and vEnd.x == 1 and vEnd.w == 1, "one colour sets both")
	oProbe:SetFade(0.25, 0.5)
	local nIn, nOut = oProbe:GetFade()
	oCheck:Expect(nIn == 0.25 and nOut == 0.5, "fades")
	oProbe:SetSpin(10, 20)
	nMin, nMax = oProbe:GetSpin()
	oCheck:Expect(nMin == 10 and nMax == 20, "spin")
	oProbe:SetShape(ParticleShape.BOX)
	oCheck:Equal(oProbe:GetShape(), ParticleShape.BOX, "shape")
	oProbe:SetShapeAngle(500)
	oCheck:Equal(oProbe:GetShapeAngle(), 180, "a cone's angle is brought into its range")
	oProbe:SetShapeSize(Vector3.new(1, 2, 3))
	oCheck:Equal(oProbe:GetShapeSize().z, 3, "box size")
	oProbe:SetGravity(Vector3.new(0, -2, 0))
	oCheck:Equal(oProbe:GetGravity().y, -2, "gravity")
	oProbe:SetDrag(0.5)
	oCheck:Equal(oProbe:GetDrag(), 0.5, "drag")
	oProbe:SetSpace(ParticleSpace.LOCAL)
	oCheck:Equal(oProbe:GetSpace(), ParticleSpace.LOCAL, "space")
	oProbe:SetUnscaled(true)
	oCheck:Expect(oProbe:IsUnscaled(), "unscaled")
	oProbe:SetUnscaled(false)
	oProbe:SetSeed(5)
	oCheck:Equal(oProbe:GetSeed(), 5, "seed")
	oProbe:SetRandomRotation(true)
	oCheck:Expect(oProbe:HasRandomRotation(), "random rotation")
	oProbe:SetDuration(3)
	oCheck:Equal(oProbe:GetDuration(), 3, "duration")
	oProbe:SetLooping(false)
	oCheck:Expect(not oProbe:IsLooping(), "looping")
	oProbe:SetPlayOnStart(false)
	oCheck:Expect(not oProbe:GetPlayOnStart(), "play on start")
	oProbe:SetBursts({ { time = 0, count = 3 }, { time = 0.5, count = 2 } })
	local tBursts = oProbe:GetBursts()
	oCheck:Expect(#tBursts == 2 and tBursts[2].time == 0.5 and tBursts[2].count == 2, "bursts")

	oProbe:SetMaxParticles(0)
	oCheck:Equal(oProbe:GetMaxParticles(), 1, "at least one particle")
	oProbe:SetMaxParticles(32)
	oCheck:Equal(oProbe:GetMaxParticles(), 32, "max particles")

	oCheck:Expect(oProbe:GetModel() == nil and oProbe:GetMaterial() == nil, "billboards and the engine's material by default")
	oProbe:SetModel(Resources.GetModel(":Models/Cube.fbx"))
	oProbe:SetMaterial(Resources.GetMaterial("Materials/ParticleMesh.ovmat"))
	oCheck:Expect(oProbe:GetModel() ~= nil and oProbe:GetMaterial() ~= nil, "a model and a material")
	oProbe:SetModel(nil)
	oProbe:SetMaterial(nil)
	oCheck:Expect(oProbe:GetModel() == nil and oProbe:GetMaterial() == nil, "back to billboards")

	-- A script's burst, stopped or not, with its overrides
	oCheck:Equal(oProbe:Emit(5, {
		position = Vector3.new(0, -49, 0), direction = Vector3.new(1, 0, 0), color = Vector4.new(1, 1, 0, 1),
		speed = 2, size = 1.5, lifetime = 100
	}), 5, "Emit returns the particles born")
	oCheck:Equal(oProbe:GetAliveCount(), 5, "they are alive at once")
	oCheck:Equal(oProbe:Emit(100), 32, "no more are born than the maximum")
	oProbe:Clear()
	oCheck:Equal(oProbe:GetAliveCount(), 0, "Clear removes them")

	oCheck:Expect(Fails(function() oProbe:Emit(-1) end), "a negative count is refused")
	oCheck:Expect(Fails(function() oProbe:Emit(1.5) end), "a count that is not whole is refused")
	oCheck:Expect(Fails(function() oProbe:Emit(2, { colour = Vector4.new(1, 1, 1, 1) }) end), "an unknown override is refused")
	oCheck:Expect(Fails(function() oProbe:Emit(2, { speed = "fast" }) end), "an override of the wrong type is refused")
	oCheck:Expect(Fails(function() oProbe:SetBursts({ 5 }) end), "a burst that is no table is refused")
	oCheck:Expect(Fails(function() oProbe:SetSeed(-1) end), "a negative seed is refused")

	oActor:RemoveParticleSystem()
	oCheck:Expect(oActor:GetParticleSystem() == nil, "removed")
end

function ParticlesCheck:OnStart()
	Drawn()
	CheckApi()

	-- What counts the rate: born at 1000 per second, living long
	t.oCounter = Actor("Counter", Vector3.new(0, -60, 0)):AddParticleSystem()
	t.oCounter:SetMaxParticles(100000)
	t.oCounter:SetRate(1000)
	t.oCounter:SetLifetime(1000)
	t.oCounter:SetShape(ParticleShape.POINT)
end

function ParticlesCheck:OnLateUpdate(nDelta)
	-- The same step as the particle systems', which advance after the late update
	nGameTime = nGameTime + nDelta
end

function ParticlesCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	-- The two emitters going round
	local nAngle = nGameTime * 2.5
	t.oRing:GetTransform():SetLocalPosition(Vector3.new(-1.5 + math.cos(nAngle) * 1.2, 3.2 + math.sin(nAngle) * 0.6, 4))
	t.oTrail:GetTransform():SetLocalPosition(Vector3.new(1.5 + math.cos(nAngle) * 1.2, 3.2 + math.sin(nAngle) * 0.6, 4))

	if nPhase == 0 and nGameTime >= 0.1 then
		-- Every 1 / rate seconds of the game's steps, one particle: no more, no less
		oCheck:Near(t.oCounter:GetAliveCount(), math.floor(1000 * nGameTime), "the rate over the frames", 1)

		-- The game paused: a game-time system waits, a real-time one goes on. The frame's step is already taken:
		-- the pause holds from the next frame
		Time.SetScale(0)
		t.oReal = Actor("Real", Vector3.new(0, -70, 0)):AddParticleSystem()
		t.oReal:SetUnscaled(true)
		t.oReal:SetRate(1000)
		t.oReal:SetLifetime(1000)
		nWaited = 0
		nPhase = 1
	elseif nPhase == 1 then
		t.nPaused = t.nPaused or t.oCounter:GetAliveCount()
		nWaited = nWaited + Time.GetUnscaledDeltaTime()

		if nWaited >= 0.05 then
			oCheck:Equal(t.oCounter:GetAliveCount(), t.nPaused, "game time stopped: nothing is born")
			oCheck:Expect(t.oReal:GetAliveCount() > 0, "real time going on: particles are born")
			Time.SetScale(1)

			t.oCounter:Stop()
			t.nStopped = t.oCounter:GetAliveCount()
			t.nStopFrame = nFrame
			nPhase = 2
		end
	elseif nPhase == 2 and nFrame >= t.nStopFrame + 3 then
		oCheck:Equal(t.oCounter:GetAliveCount(), t.nStopped, "stopped: the particles live on, none is born")
		t.oCounter:Clear()
		oCheck:Equal(t.oCounter:GetAliveCount(), 0, "cleared")
		nPhase = 3
	elseif nPhase == 3 and nFrame >= 120 then
		-- Drawn for 120 frames: every pass has drawn the systems
		oCheck:Finish()
	end
end

return ParticlesCheck
