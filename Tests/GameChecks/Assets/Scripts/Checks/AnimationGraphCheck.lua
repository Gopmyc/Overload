-- An animation graph on the engine's mannequin (Animations/Mannequin.ovanim): a locomotion blend along the speed and a
-- jump, under an arms layer masked above spine_02 that rests empty or punches on a trigger. With time stopped, the
-- graph's pose is checked bone by bone against twins playing the same clips on their own layers: the idle at rest, the
-- walk and the run in equal parts halfway between them, the punch on the arms over the legs' blend. With time running:
-- the jump taken on its trigger and left at its exit time, the punch, the layer weight, the parameters refused by name
-- and kind, files that cannot play named, and the renderer's own layers back once the animator is removed
local Check = require("Scripts/Checks/Check")

local AnimationGraphCheck = {}
local oCheck = Check.New("AnimationGraph")
local nFrame = 0
local nPhase = 0
local nWaited = 0
local bDone = false
local t = {}

local ARM_BONES = { "spine_02", "neck_01", "head", "clavicle.r", "upperarm.r", "lowerarm.r", "hand.r", "hand.l" }
local LEG_BONES = { "pelvis", "spine_01", "thigh.l", "calf.l", "foot.l", "thigh.r", "calf.r", "foot.r" }

local function Mannequin(sName, vPosition)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vPosition)
	oActor:AddModelRenderer():SetModel(Resources.GetModel(":Models/Mannequin.fbx"))
	oActor:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))
	return oActor, oActor:AddSkinnedMeshRenderer()
end

-- A twin paused at the start of a clip, on its first layer
local function Hold(oRenderer, sClip, nLayer)
	oRenderer:SetAnimation(sClip, nLayer or 0)
	oRenderer:Pause(nLayer or 0)
	oRenderer:SetTime(0, nLayer or 0)
end

local function SameBone(oA, oB, sBone)
	local nA, nB = oA:GetBoneIndex(sBone), oB:GetBoneIndex(sBone)
	local qA, qB = oA:GetBoneLocalRotation(nA), oB:GetBoneLocalRotation(nB)
	local vA, vB = oA:GetBoneLocalPosition(nA), oB:GetBoneLocalPosition(nB)
	return math.abs(math.abs(qA.x * qB.x + qA.y * qB.y + qA.z * qB.z + qA.w * qB.w) - 1) < 1e-5 and Vector3.Distance(vA, vB) < 1e-5
end

local function SameBones(oA, oB, tBones)
	for _, sBone in ipairs(tBones) do
		if not SameBone(oA, oB, sBone) then
			return false, sBone
		end
	end
	return true
end

local function EveryBone(oA, oB)
	local tBones = {}
	for i = 0, oA:GetBoneCount() - 1 do
		tBones[#tBones + 1] = oA:GetBoneName(i)
	end
	return SameBones(oA, oB, tBones)
end

local function Mentions(sText, sPart)
	return type(sText) == "string" and sText:find(sPart, 1, true) ~= nil
end

local function Fails(fCall)
	return not pcall(fCall)
end

function AnimationGraphCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:GetTransform():SetLocalPosition(Vector3.new(1, 1.2, 5))
	oCamera:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(0, 180, 0)))
	oCamera:AddCamera()
	local oSun = oScene:CreateActor("Sun", "")
	oSun:AddDirectionalLight()
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(30, 160, 0)))
	oScene:CreateActor("Ambient", ""):AddAmbientSphereLight():SetRadius(100)

	t.oDrivenActor, t.oDriven = Mannequin("Driven", Vector3.new(0, 0, 0))
	t.oAnimator = t.oDrivenActor:AddAnimator()
	t.oAnimator:SetGraphPath("Animations/Mannequin.ovanim")

	t.oTwinActor, t.oTwin = Mannequin("Twin", Vector3.new(1.2, 0, 0))
	t.oPunchActor, t.oPunch = Mannequin("PunchTwin", Vector3.new(2.4, 0, 0))

	local oBrokenActor = Mannequin("Broken", Vector3.new(-1.2, 0, 0))
	t.oBroken = oBrokenActor:AddAnimator()
	t.oBroken:SetGraphPath("Animations/UnknownClip.ovanim")

	local oMissingActor = Mannequin("Missing", Vector3.new(-2.4, 0, 0))
	t.oMissing = oMissingActor:AddAnimator()
	t.oMissing:SetGraphPath("Animations/Nowhere.ovanim")
end

function AnimationGraphCheck:OnUpdate()
	nFrame = nFrame + 1
	nWaited = nWaited + Time.GetUnscaledDeltaTime()
	if bDone or nFrame < 2 then
		return
	end

	local oAnimator = t.oAnimator

	if nPhase == 0 then
		Time.SetScale(0)

		oCheck:Expect(oAnimator:IsLoaded(), "the graph is loaded: " .. tostring(oAnimator:GetError()))
		oCheck:Equal(oAnimator:GetError(), nil, "nothing is wrong")
		local tLayers = oAnimator:GetLayerNames()
		oCheck:Expect(#tLayers == 2 and tLayers[1] == "Base" and tLayers[2] == "Arms", "the layers read back")
		local tParameters = oAnimator:GetParameterNames()
		oCheck:Expect(#tParameters == 3 and tParameters[1] == "Speed" and tParameters[3] == "Punch", "the parameters read back")
		oCheck:Equal(oAnimator:GetState(), "Locomotion", "the base layer starts in its first state")
		oCheck:Equal(oAnimator:GetState("Arms"), "Rest", "a layer is found by name")
		oCheck:Equal(oAnimator:GetState(1), "Rest", "or by index, from 0")
		oCheck:Equal(oAnimator:GetState("Legs"), nil, "an unknown layer has no state")
		oCheck:Expect(Fails(function() oAnimator:GetState({}) end), "a layer that is neither an index nor a name is refused")

		oCheck:Expect(not oAnimator:SetFloat("Nope", 1), "an unknown parameter is refused")
		oCheck:Expect(not oAnimator:SetBool("Speed", true), "a parameter of another kind is refused")
		oCheck:Expect(not oAnimator:SetTrigger("Speed"), "a number is no trigger")
		oCheck:Equal(oAnimator:GetFloat("Speed"), 0, "a number reads back its default")
		oCheck:Equal(oAnimator:GetBool("Speed"), nil, "a number is no boolean")

		oCheck:Expect(not t.oBroken:IsLoaded(), "a graph naming a clip the model lacks does not play")
		oCheck:Expect(Mentions(t.oBroken:GetError(), "layers[0].states[0].clip: no clip 'root|moonwalk'"), "and says where: " .. tostring(t.oBroken:GetError()))
		oCheck:Expect(Mentions(t.oMissing:GetError(), "cannot read 'Animations/Nowhere.ovanim'"), "a missing file is named: " .. tostring(t.oMissing:GetError()))

		-- At rest, from the start of the idle
		oAnimator:Reset()
		Hold(t.oTwin, "root|idle")
		nPhase = 1
	elseif nPhase == 1 then
		local bSame, sBone = EveryBone(t.oDriven, t.oTwin)
		oCheck:Expect(bSame, "at rest, the graph's pose is the idle clip's, bone for bone" .. (sBone and (": not " .. sBone) or ""))

		-- Halfway between the walk (1.5) and the run (4)
		oCheck:Expect(oAnimator:SetFloat("Speed", 2.75), "a number is set")
		oCheck:Equal(oAnimator:GetFloat("Speed"), 2.75, "and reads back")
		Hold(t.oTwin, "root|walk", 0)
		t.oTwin:AddLayer()
		t.oTwin:SetLayerWeight(1, 1)
		Hold(t.oTwin, "root|run", 1)
		nPhase = 2
	elseif nPhase == 2 then
		local bSame, sBone = EveryBone(t.oDriven, t.oTwin)
		oCheck:Expect(bSame, "halfway between them, the blend is the walk's and the run's in equal parts" .. (sBone and (": not " .. sBone) or ""))

		-- The arms punch over the legs' blend
		oCheck:Expect(oAnimator:Play("Punch", 0, "Arms"), "a state is played in a layer")
		oCheck:Equal(oAnimator:GetState("Arms"), "Punch", "and is the layer's state")
		oCheck:Expect(not oAnimator:Play("Dance"), "an unknown state is refused")
		Hold(t.oPunch, "root|fight_punch")
		nPhase = 3
	elseif nPhase == 3 then
		local bArms, sArm = SameBones(t.oDriven, t.oPunch, ARM_BONES)
		local bLegs, sLeg = SameBones(t.oDriven, t.oTwin, LEG_BONES)
		oCheck:Expect(bArms, "above its mask, the arms layer's punch replaces the base" .. (sArm and (": not " .. sArm) or ""))
		oCheck:Expect(bLegs, "below it, the legs keep the base's blend" .. (sLeg and (": not " .. sLeg) or ""))

		-- Time runs again: a jump on its trigger
		Time.SetScale(1)
		oAnimator:Play("Rest", 0, "Arms")
		oAnimator:SetFloat("Speed", 0)
		oCheck:Expect(oAnimator:SetTrigger("Jump"), "a trigger is set")
		nPhase, nWaited = 4, 0
	elseif nPhase == 4 then
		oCheck:Equal(oAnimator:GetState(), "Jump", "the trigger takes the base layer into the jump")
		oCheck:Expect(oAnimator:IsInTransition(), "fading into it")
		oCheck:Expect(not oAnimator:IsInTransition("Arms"), "while the arms rest")
		nPhase, nWaited = 5, 0
	elseif nPhase == 5 then
		if oAnimator:GetState() == "Locomotion" then
			oCheck:Expect(nWaited > 0.3, "the jump plays out before its exit time: " .. string.format("%.2f", nWaited) .. " s")
			oCheck:Expect(oAnimator:SetTrigger("Punch"), "the punch's trigger is set")
			nPhase, nWaited = 6, 0
		elseif nWaited > 6 then
			oCheck:Expect(false, "the jump ends at its exit time, back in locomotion")
			nPhase, nWaited = 6, 0
		end
	elseif nPhase == 6 then
		oCheck:Equal(oAnimator:GetState("Arms"), "Punch", "the punch's trigger takes the arms into it, from any state")
		nPhase, nWaited = 7, 0
	elseif nPhase == 7 then
		if oAnimator:GetState("Arms") == "Rest" then
			oCheck:Expect(nWaited > 0.2, "the punch plays out before the arms rest again")

			oCheck:Expect(oAnimator:SetLayerWeight("Arms", 0.5), "a layer's weight is set")
			oCheck:Near(oAnimator:GetLayerWeight("Arms"), 0.5, "and reads back", 1e-6)
			oCheck:Expect(not oAnimator:SetLayerWeight("Legs", 1), "an unknown layer's weight is refused")

			-- Without its animator, the renderer plays its own layer again
			Time.SetScale(0)
			Hold(t.oDriven, "root|walk")
			t.oDrivenActor:RemoveAnimator()
			t.oTwin:RemoveLayer(1)
			Hold(t.oTwin, "root|walk")
			nPhase = 8
		elseif nWaited > 6 then
			oCheck:Expect(false, "the punch ends, and the arms rest again")
			nPhase = 9
		end
	elseif nPhase == 8 then
		local bSame, sBone = EveryBone(t.oDriven, t.oTwin)
		oCheck:Expect(bSame, "once the animator is removed, the renderer plays its own layer again" .. (sBone and (": not " .. sBone) or ""))
		nPhase = 9
	elseif nPhase == 9 then
		Time.SetScale(1)
		bDone = true
		oCheck:Finish()
	end
end

return AnimationGraphCheck
