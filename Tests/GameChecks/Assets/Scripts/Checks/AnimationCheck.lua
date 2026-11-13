-- Skinned mesh poses worked out on demand: a setting then a bone, and a bone then a setting, in the order they are
-- made; many writes in a frame giving one skeleton; and a model set to update when visible that, out of sight, lets
-- its clip run without working its pose out, still answers a script's read rightly, keeps its attachment still, and
-- is worked out again once back in view
local Check = require("Scripts/Checks/Check")

local AnimationCheck = {}
local oCheck = Check.New("Animation")
local nFrame = 0
local nPhase = 0
local nWaited = 0
local t = {}

local function Mannequin(sName, vPosition)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vPosition)
	oActor:AddModelRenderer():SetModel(Resources.GetModel(":Models/Mannequin.fbx"))
	oActor:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))

	local oRenderer = oActor:AddSkinnedMeshRenderer()
	oRenderer:SetAnimation("root|walk")

	-- A prop carried in the right hand, placed by the engine on its bone
	local oProp = Scenes.GetCurrentScene():CreateActor(sName .. " Prop", "")
	oProp:GetTransform():SetLocalScale(Vector3.new(0.1, 0.1, 0.1))
	oProp:AddModelRenderer():SetModel(Resources.GetModel(":Models/Cube.fbx"))
	oProp:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial("Materials/Ground.ovmat"))
	local oAttachment = oProp:AddBoneAttachment()
	oAttachment:SetTarget(oActor)
	oAttachment:SetBone("hand.r")

	return oRenderer, oProp
end

local function Same(qA, qB)
	return math.abs(qA.x - qB.x) < 1e-4 and math.abs(qA.y - qB.y) < 1e-4 and math.abs(qA.z - qB.z) < 1e-4 and math.abs(qA.w - qB.w) < 1e-4
end

local function PropPosition(oProp)
	return oProp:GetTransform():GetWorldPosition()
end

function AnimationCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:GetTransform():SetLocalPosition(Vector3.new(0, 1, -4))
	oCamera:AddCamera()

	oScene:CreateActor("Sun", ""):AddDirectionalLight()
	oScene:CreateActor("Ambient", ""):AddAmbientSphereLight():SetRadius(100)

	-- One in front of the camera, one far behind it, out of every pass
	t.oSeen, t.oSeenProp = Mannequin("Seen", Vector3.new(0, 0, 0))
	t.oHidden, t.oHiddenProp = Mannequin("Hidden", Vector3.new(0, 0, -300))
	t.oHidden:SetUpdateMode(AnimationUpdateMode.WHEN_VISIBLE)
	t.nHead = t.oSeen:GetBoneIndex("head")
	t.nHand = t.oSeen:GetBoneIndex("hand.r")
end

function AnimationCheck:OnUpdate()
	nFrame = nFrame + 1
	nWaited = nWaited + Time.GetUnscaledDeltaTime()
	local oSeen, oHidden = t.oSeen, t.oHidden

	if nFrame == 2 then
		oCheck:Equal(oSeen:GetUpdateMode(), AnimationUpdateMode.ALWAYS, "the default update mode")
		oCheck:Equal(oHidden:GetUpdateMode(), AnimationUpdateMode.WHEN_VISIBLE, "the update mode set")
		oCheck:Expect(oSeen:GetActiveAnimationName() == "root|walk" and oSeen:IsPlaying(), "the clip plays")

		-- A setting then a bone: the bone, written after, wins
		local qTurn = Quaternion.new(Vector3.new(0, 40, 0))
		oSeen:SetLayerWeight(0.5)
		oSeen:SetBoneLocalRotation(t.nHead, qTurn)
		oCheck:Expect(Same(oSeen:GetBoneLocalRotation(t.nHead), qTurn), "a weight then a bone: the bone")

		-- A bone then a setting: the setting's pose, as when every setting worked the pose out at once
		oSeen:SetBoneLocalRotation(t.nHead, qTurn)
		oSeen:SetLayerWeight(1)
		oCheck:Expect(not Same(oSeen:GetBoneLocalRotation(t.nHead), qTurn), "a bone then a weight: the clip's pose")

		-- Many writes and settings in a frame work nothing out until something needs it
		local nSkeletons, nPoses = oSeen:GetSkeletonUpdateCount(), oSeen:GetPoseEvaluationCount()
		for i = 1, 1000 do
			oSeen:SetBoneLocalRotation(t.nHead, Quaternion.new(Vector3.new(0, i % 40, 0)))
		end
		for i = 1, 100 do
			oSeen:SetLayerWeight(i % 2 == 0 and 1 or 0.9)
		end
		oCheck:Equal(oSeen:GetSkeletonUpdateCount(), nSkeletons, "a thousand bone writes work no skeleton out")
		oCheck:Equal(oSeen:GetPoseEvaluationCount(), nPoses, "a hundred weights sample no clip")
		t.nSkeletons, t.nPoses = nSkeletons, nPoses
	elseif nFrame == 3 then
		-- The scene worked them out once, after the scripts
		oCheck:Equal(oSeen:GetSkeletonUpdateCount() - t.nSkeletons, 1, "the frame's writes gave one skeleton")
		oCheck:Equal(oSeen:GetPoseEvaluationCount() - t.nPoses, 1, "the frame's weights gave one pose")
	elseif nFrame == 5 then
		oCheck:Expect(oSeen:WasVisible(), "the model in view was drawn")
		oCheck:Expect(not oHidden:WasVisible(), "the model behind the camera was not")

		t.nHiddenPoses = oHidden:GetPoseEvaluationCount()
		t.nSeenPoses = oSeen:GetPoseEvaluationCount()
		t.nHiddenTime = oHidden:GetTime()
		t.vHiddenProp = PropPosition(t.oHiddenProp)
		t.vSeenProp = PropPosition(t.oSeenProp)
		nPhase, nWaited = 1, 0
	elseif nPhase == 1 and nWaited >= 0.5 then
		-- Half a second on (frames are as short as the machine allows): out of sight, the clip runs on, but no pose
		-- is worked out and the prop stays where it was; in view, poses come at the evaluation rate (60 a second)
		oCheck:Equal(oHidden:GetPoseEvaluationCount(), t.nHiddenPoses, "out of sight, no pose is worked out")
		oCheck:Expect(oHidden:GetTime() ~= t.nHiddenTime, "out of sight, the clip runs on")
		oCheck:Expect(Vector3.Distance(PropPosition(t.oHiddenProp), t.vHiddenProp) < 1e-5, "out of sight, the prop stays")
		oCheck:Expect(oSeen:GetPoseEvaluationCount() - t.nSeenPoses >= 10, "in view, the pose is worked out as the clip runs")
		oCheck:Expect(Vector3.Distance(PropPosition(t.oSeenProp), t.vSeenProp) > 1e-3, "in view, the prop moves with the hand")

		-- A script reading a bone of the hidden model gets the right pose: the same clip at the same time on the same
		-- model in view gives the same bone
		oSeen:Pause()
		oHidden:Pause()
		oSeen:SetTime(0.4)
		oHidden:SetTime(0.4)
		local vSeen = oSeen:GetBoneGlobalPosition(t.nHand)
		local vHidden = oHidden:GetBoneGlobalPosition(t.nHand)
		oCheck:Expect(vSeen ~= nil and vHidden ~= nil and Vector3.Distance(vSeen, vHidden) < 1e-4, "a bone read out of sight is right")
		oCheck:Expect(oHidden:GetPoseEvaluationCount() > t.nHiddenPoses, "the read worked the pose out")

		-- Back in view
		oSeen:Play()
		oHidden:Play()
		oHidden:GetOwner():GetTransform():SetLocalPosition(Vector3.new(1.5, 0, 0))
		nPhase, nWaited = 2, 0
	elseif nPhase == 2 and nWaited >= 0.1 then
		oCheck:Expect(oHidden:WasVisible(), "back in view, it is drawn")
		t.nHiddenPoses = oHidden:GetPoseEvaluationCount()
		t.vHiddenProp = PropPosition(t.oHiddenProp)
		nPhase, nWaited = 3, 0
	elseif nPhase == 3 and nWaited >= 0.5 then
		oCheck:Expect(oHidden:GetPoseEvaluationCount() - t.nHiddenPoses >= 10, "back in view, its pose is worked out again")
		oCheck:Expect(Vector3.Distance(PropPosition(t.oHiddenProp), t.vHiddenProp) > 1e-3, "back in view, its prop moves again")
		nPhase = 4
		oCheck:Finish()
	end
end

return AnimationCheck
