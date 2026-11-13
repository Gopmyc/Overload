-- Look at and spring bones, over the animation. A mannequin looks at a target, its head the whole way and its spine and
-- neck a share, against a twin that plays the same pose without; within its bounds; holding its direction as its body
-- turns; faded out by its weight. A rope hanging beside a post (Models/Rigs/Post.gltf, from make_rigs.py) swings back
-- as the post starts off, stays out of the post where a twin without the collider goes through it, comes back under
-- once the post stops, starts again from rest after a jump, stays above a ground, and rests out of sight. Wrong bones
-- and settings are named. It also draws them from the front, for a screenshot (--frames 30 --screenshot)
local Check = require("Scripts/Checks/Check")

local AnimationConstraintsCheck = {}
local oCheck = Check.New("AnimationConstraints")
local nFrame = 0
local bDone = false
local tLook = { nPhase = 0, nWaited = 0 }
local tSpring = { nPhase = 0, nWaited = 0 }

local ROPE_OFFSET = Vector3.new(0.15, 0, 0)
local REACH = 0.1 + 0.025

local function Fails(fCall)
	local bOk = pcall(fCall)
	return not bOk
end

local function Mentions(sText, sPart)
	return type(sText) == "string" and sText:find(sPart, 1, true) ~= nil
end

local function Conjugate(q)
	return Quaternion.new(-q.x, -q.y, -q.z, q.w)
end

-- The yaw (towards +X) and pitch (up) of a direction, degrees
local function YawPitch(v)
	return math.deg(math.atan(v.x, v.z)), math.deg(math.asin(math.max(-1, math.min(1, v.y / v:Length()))))
end

-- Where a bone's turn over the twin's points the front (+Z), in the model's space
local function TurnedFront(sBone)
	local nBone = tLook.oLooker:GetBoneIndex(sBone)
	local qTurn = tLook.oLooker:GetBoneGlobalRotation(nBone) * Conjugate(tLook.oTwin:GetBoneGlobalRotation(nBone))
	return Quaternion.RotatePoint(Vector3.new(0, 0, 1), qTurn)
end

-- The head's joint in the world as the animation alone puts it: the twin's, on the looker
local function BaseHead()
	return tLook.oLookerActor:GetTransform():GetWorldPosition() + tLook.oTwin:GetBoneGlobalPosition(tLook.nHead)
end

local function Mannequin(sName, vPosition)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vPosition)
	oActor:AddModelRenderer():SetModel(Resources.GetModel(":Models/Mannequin.fbx"))
	oActor:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))
	local oRenderer = oActor:AddSkinnedMeshRenderer()
	oRenderer:SetAnimation("root|walk")
	oRenderer:Pause()
	oRenderer:SetTime(0.3)
	return oActor, oRenderer
end

local function Post(sName, vPosition)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vPosition)
	oActor:AddModelRenderer():SetModel(Resources.GetModel("Models/Rigs/Post.gltf"))
	oActor:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))
	local oRenderer = oActor:AddSkinnedMeshRenderer()
	local oSprings = oActor:AddSpringBones()
	oSprings:SetGroup("Rope")
	oSprings:SetGroupChains("Rope", { "rope_1" })
	return oActor, oSprings, oRenderer
end

function AnimationConstraintsCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:GetTransform():SetLocalPosition(Vector3.new(1.2, 1.4, 4.5))
	oCamera:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(0, 180, 0)))
	oCamera:AddCamera()
	local oSun = oScene:CreateActor("Sun", "")
	oSun:AddDirectionalLight()
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(30, 160, 0)))
	oScene:CreateActor("Ambient", ""):AddAmbientSphereLight():SetRadius(100)

	tLook.oLookerActor, tLook.oLooker = Mannequin("Looker", Vector3.new(0, 0, 0))
	tLook.oTwinActor, tLook.oTwin = Mannequin("Twin", Vector3.new(-1.5, 0, 0))
	tLook.nHead = tLook.oLooker:GetBoneIndex("head")
	tLook.oLook = tLook.oLookerActor:AddLookAt()

	tLook.oTarget = oScene:CreateActor("Target", "")
	tLook.oTarget:GetTransform():SetLocalScale(Vector3.new(0.1, 0.1, 0.1))
	tLook.oTarget:AddModelRenderer():SetModel(Resources.GetModel(":Models/Cube.fbx"))
	tLook.oTarget:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))

	-- The rope meets its post; the loose one's does not
	tSpring.oActor, tSpring.oSprings, tSpring.oRenderer = Post("Rope", Vector3.new(2, 0, 0))
	tSpring.oSprings:SetCollider("Post", { from = "post", to = "post_top", radius = 0.1 })
	tSpring.oSprings:SetGroupColliders("Rope", { "Post" })
	tSpring.oLooseActor, tSpring.oLoose = Post("Loose", Vector3.new(4, 0, 0))
	tSpring.nRope2 = tSpring.oRenderer:GetBoneIndex("rope_2")
end

-- Puts the target at a yaw and a pitch from the head, 2 m on, in the looker's space (unturned)
local function PlaceTarget(nYaw, nPitch)
	local nY, nP = math.rad(nYaw), math.rad(nPitch)
	local vDirection = Vector3.new(math.sin(nY) * math.cos(nP), math.sin(nP), math.cos(nY) * math.cos(nP))
	tLook.oTarget:GetTransform():SetLocalPosition(BaseHead() + vDirection * 2)
end

local function StepLook(nDt)
	local oLook = tLook.oLook
	tLook.nWaited = tLook.nWaited + nDt

	if tLook.nPhase == 0 then
		oCheck:Expect(Fails(function() oLook:SetBones({ "head" }) end), "bones that are no { bone, weight } pairs are refused")
		oLook:SetBones({ { "spine_02", 0.2 }, { "neck_01", 0.3 }, { "head", 0.5 } })
		local tBones = oLook:GetBones()
		oCheck:Expect(#tBones == 3 and tBones[1][1] == "spine_02" and tBones[3][1] == "head" and math.abs(tBones[2][2] - 0.3) < 1e-6, "the bones read back")
		oCheck:Expect(oLook:IsAtRest(), "without a target, the look rests")

		PlaceTarget(40, 0)
		oLook:SetTarget(tLook.oTarget)
		oCheck:Expect(oLook:HasTarget() and oLook:GetTarget() == tLook.oTarget, "the target reads back")
		tLook.nPhase, tLook.nWaited = 1, 0
	elseif tLook.nPhase == 1 and tLook.nWaited >= 1.5 then
		oCheck:Equal(oLook:GetError(), nil, "nothing is wrong")
		local nYaw, nPitch = YawPitch(tLook.oTarget:GetTransform():GetWorldPosition() - BaseHead())
		oCheck:Near(oLook:GetYaw(), nYaw, "the look's yaw reaches the target's", 0.2)
		oCheck:Near(oLook:GetPitch(), nPitch, "the look's pitch reaches the target's", 0.2)
		oCheck:Expect(not oLook:IsAtRest(), "looking at something, the look is not at rest")

		-- The head turns the whole way, the bones before it their share
		local nHeadYaw, nHeadPitch = YawPitch(TurnedFront("head"))
		oCheck:Near(nHeadYaw, oLook:GetYaw(), "the head looks the whole way", 0.05)
		oCheck:Near(nHeadPitch, oLook:GetPitch(), "the head looks up or down the whole way", 0.05)
		oCheck:Near((YawPitch(TurnedFront("neck_01"))), oLook:GetYaw() * 0.5, "the neck turns half the way, its share and the spine's", 0.05)
		oCheck:Near((YawPitch(TurnedFront("spine_02"))), oLook:GetYaw() * 0.2, "the spine turns its share", 0.05)

		local vToTarget = (tLook.oTarget:GetTransform():GetWorldPosition() - BaseHead()):Normalize()
		oCheck:Expect(oLook:GetLookDirection():Dot(vToTarget) > math.cos(math.rad(0.3)), "the look's direction points at the target")

		-- Behind, past the bounds
		PlaceTarget(-150, 0)
		tLook.nPhase, tLook.nWaited = 2, 0
	elseif tLook.nPhase == 2 and tLook.nWaited >= 1.5 then
		oCheck:Near(oLook:GetYaw(), -70, "behind, the look goes as far as its bound", 0.05)
		oLook:SetMaxYaw(50)
		oCheck:Near(oLook:GetMaxYaw(), 50, "the bound reads back", 1e-4)
		PlaceTarget(0, 80)
		tLook.nPhase, tLook.nWaited = 3, 0
	elseif tLook.nPhase == 3 and tLook.nWaited >= 1.5 then
		oCheck:Near(oLook:GetPitch(), 40, "high above, the look goes as far up as its bound", 0.05)
		oCheck:Near(oLook:GetYaw(), 0, "and no way aside", 0.05)
		PlaceTarget(30, 0)
		tLook.nPhase, tLook.nWaited = 4, 0
	elseif tLook.nPhase == 4 and tLook.nWaited >= 1.5 then
		-- The body turns 20 degrees towards the target: the look holds its direction in the world at once
		tLook.vBefore = oLook:GetLookDirection()
		tLook.oLookerActor:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(0, 20, 0)))
		tLook.nPhase, tLook.nWaited = 5, 0
	elseif tLook.nPhase == 5 then
		-- A frame on
		local nOff = math.deg(math.acos(math.max(-1, math.min(1, oLook:GetLookDirection():Dot(tLook.vBefore)))))
		oCheck:Expect(nOff < 1, "as the body turns, the look holds its direction: " .. string.format("%.2f", nOff) .. " degrees off")
		oCheck:Near(oLook:GetYaw(), 10, "the look turned back under it", 1)

		oLook:SetWeight(0)
		oCheck:Near(oLook:GetWeight(), 0, "the weight reads back", 1e-6)
		tLook.nPhase, tLook.nWaited = 6, 0
	elseif tLook.nPhase == 6 and tLook.nWaited >= 0.1 and not tLook.bFading then
		tLook.bFading = true
		local nCurrent = oLook:GetCurrentWeight()
		oCheck:Expect(nCurrent > 0 and nCurrent < 1, "the weight fades: " .. string.format("%.2f", nCurrent))
	elseif tLook.nPhase == 6 and tLook.nWaited >= 0.6 then
		oCheck:Near(oLook:GetCurrentWeight(), 0, "the weight has faded out", 1e-6)
		local vHead = TurnedFront("head")
		oCheck:Expect(vHead:Dot(Vector3.new(0, 0, 1)) > 1 - 1e-6, "faded out, the head is the animation's")

		oLook:SetWeight(1)
		oLook:ClearTarget()
		oCheck:Expect(not oLook:HasTarget(), "the target is cleared")
		tLook.nPhase, tLook.nWaited = 7, 0
	elseif tLook.nPhase == 7 and tLook.nWaited >= 2 then
		oCheck:Expect(oLook:IsAtRest(), "without a target, the look goes back to the front and rests")
		oCheck:Expect(TurnedFront("head"):Dot(Vector3.new(0, 0, 1)) > 1 - 1e-5, "at rest, the head is the animation's")

		oLook:SetBones({ { "nope", 1 } })
		tLook.nPhase, tLook.nWaited = 8, 0
	elseif tLook.nPhase == 8 then
		oCheck:Expect(Mentions(oLook:GetError(), "'nope'"), "an unknown bone is named: " .. tostring(oLook:GetError()))
		oLook:SetBones({ { "hand.r", 1 }, { "head", 1 } })
		tLook.nPhase = 9
	elseif tLook.nPhase == 9 then
		oCheck:Expect(Mentions(oLook:GetError(), "chain"), "bones that make no chain are refused: " .. tostring(oLook:GetError()))
		tLook.nPhase = 10
	end
end

-- Every joint's distance to its post's axis, the least of them
local function NearestToPost(oActor, oSprings)
	local vBase = oActor:GetTransform():GetWorldPosition()
	local nLeast = math.huge
	for nBone = 0, 2 do
		local v = oSprings:GetJointPosition(0, nBone) - vBase
		if v.y >= 0 and v.y <= 1.5 then
			nLeast = math.min(nLeast, math.sqrt(v.x * v.x + v.z * v.z))
		end
	end
	return nLeast
end

local function StepSpring(nDt)
	local oSprings = tSpring.oSprings
	tSpring.nWaited = tSpring.nWaited + nDt

	if tSpring.nPhase == 0 and tSpring.nWaited >= 0.3 then
		oCheck:Equal(oSprings:GetError(), nil, "the rope is made")
		oCheck:Equal(oSprings:GetChainCount(), 1, "one chain")
		local vBase = tSpring.oActor:GetTransform():GetWorldPosition()
		oCheck:Expect(Vector3.Distance(oSprings:GetJointPosition(0, 2), vBase + ROPE_OFFSET + Vector3.new(0, 0.3, 0)) < 1e-3, "at rest, the rope hangs straight down")
		oCheck:Equal(oSprings:GetJointPosition(0, 3), nil, "a bone past the chain has no joint")

		local tNames = oSprings:GetGroupNames()
		oCheck:Expect(#tNames == 1 and tNames[1] == "Rope", "the group reads back")
		oCheck:Equal(oSprings:GetGroupColliders("Rope")[1], "Post", "the group's collider reads back")
		oCheck:Near(oSprings:GetGroupSettings("Rope").stiffness, 150, "the group's settings read back, defaults first", 1e-4)

		tSpring.nLeast, tSpring.nLooseLeast, tSpring.nLag, tSpring.bApplied = math.huge, math.huge, 0, true
		tSpring.nPhase, tSpring.nWaited = 1, 0
	elseif tSpring.nPhase == 1 then
		-- The bones as drawn are the bones as simulated (last frame's, the post not moved yet this one)
		local vRendered = tSpring.oRenderer:GetBoneGlobalPosition(tSpring.nRope2) + tSpring.oActor:GetTransform():GetWorldPosition()
		tSpring.bApplied = tSpring.bApplied and Vector3.Distance(vRendered, oSprings:GetJointPosition(0, 0)) < 1e-4

		tSpring.nLeast = math.min(tSpring.nLeast, NearestToPost(tSpring.oActor, oSprings))
		tSpring.nLooseLeast = math.min(tSpring.nLooseLeast, NearestToPost(tSpring.oLooseActor, tSpring.oLoose))
		local vLooseTip = tSpring.oLoose:GetJointPosition(0, 2) - tSpring.oLooseActor:GetTransform():GetWorldPosition() - ROPE_OFFSET
		tSpring.nLag = math.min(tSpring.nLag, vLooseTip.x)

		-- Both posts start off at 3 m/s, for 0.4 s
		if tSpring.nWaited < 0.4 then
			for _, oActor in ipairs({ tSpring.oActor, tSpring.oLooseActor }) do
				local oTransform = oActor:GetTransform()
				oTransform:SetLocalPosition(oTransform:GetLocalPosition() + Vector3.new(3 * nDt, 0, 0))
			end
		elseif tSpring.nWaited >= 3 then
			oCheck:Expect(tSpring.bApplied, "the bones are turned as simulated, frame after frame")
			oCheck:Expect(tSpring.nLag < -0.05, "as the post starts off, a loose rope's end swings back: " .. string.format("%.3f", tSpring.nLag) .. " m")
			oCheck:Expect(tSpring.nLooseLeast < REACH - 0.02, "and goes into its post, meeting no collider: " .. string.format("%.3f", tSpring.nLooseLeast) .. " m off its axis")
			oCheck:Expect(tSpring.nLeast > REACH - 0.005 and tSpring.nLeast < REACH + 0.01, "the rope that meets its post comes against it, and stays out: " .. string.format("%.3f", tSpring.nLeast) .. " m off its axis")
			local vTip = oSprings:GetJointPosition(0, 2) - tSpring.oActor:GetTransform():GetWorldPosition()
			oCheck:Expect(Vector3.Distance(vTip, ROPE_OFFSET + Vector3.new(0, 0.3, 0)) < 0.01, "once the post stops, the rope comes back under")

			-- A jump: the rope starts again from rest there
			local oTransform = tSpring.oActor:GetTransform()
			oTransform:SetLocalPosition(oTransform:GetLocalPosition() + Vector3.new(0, 0, 20))
			tSpring.nPhase, tSpring.nWaited = 2, 0
		end
	elseif tSpring.nPhase == 2 then
		local vTip = oSprings:GetJointPosition(0, 2) - tSpring.oActor:GetTransform():GetWorldPosition()
		oCheck:Expect(Vector3.Distance(vTip, ROPE_OFFSET + Vector3.new(0, 0.3, 0)) < 1e-3, "after a jump, the rope starts again from rest")
		local oTransform = tSpring.oActor:GetTransform()
		oTransform:SetLocalPosition(oTransform:GetLocalPosition() - Vector3.new(0, 0, 20))

		oSprings:SetGround(true, 0.6)
		oCheck:Expect(oSprings:IsGroundEnabled(), "the ground reads back")
		oCheck:Near(oSprings:GetGroundOffset(), 0.6, "the ground's offset reads back", 1e-6)
		tSpring.nPhase, tSpring.nWaited = 3, 0
	elseif tSpring.nPhase == 3 and tSpring.nWaited >= 1 then
		local nLowest = math.huge
		for nBone = 0, 2 do
			nLowest = math.min(nLowest, (oSprings:GetJointPosition(0, nBone) - tSpring.oActor:GetTransform():GetWorldPosition()).y)
		end
		oCheck:Expect(nLowest >= 0.6 + 0.025 - 1e-3, "no joint under the ground: the lowest at " .. string.format("%.3f", nLowest) .. " m")
		oSprings:SetGround(false)

		-- Far aside, out of every pass
		local oTransform = tSpring.oActor:GetTransform()
		oTransform:SetLocalPosition(oTransform:GetLocalPosition() + Vector3.new(300, 0, 0))
		tSpring.nPhase, tSpring.nWaited = 4, 0
	elseif tSpring.nPhase == 4 and tSpring.nWaited >= 0.2 then
		oCheck:Expect(oSprings:IsAsleep(), "out of sight, the rope rests")
		local oTransform = tSpring.oActor:GetTransform()
		oTransform:SetLocalPosition(oTransform:GetLocalPosition() - Vector3.new(300, 0, 0))
		tSpring.nPhase, tSpring.nWaited = 5, 0
	elseif tSpring.nPhase == 5 and tSpring.nWaited >= 0.2 then
		oCheck:Expect(not oSprings:IsAsleep(), "back in view, it wakes")
		local vTip = oSprings:GetJointPosition(0, 2) - tSpring.oActor:GetTransform():GetWorldPosition()
		oCheck:Expect(Vector3.Distance(vTip, ROPE_OFFSET + Vector3.new(0, 0.3, 0)) < 1e-3, "and starts from rest")

		-- Wrong settings, named
		oCheck:Expect(Fails(function() oSprings:SetGroup("Bad", { stifness = 1 }) end), "an unknown setting is refused")
		oCheck:Expect(Fails(function() oSprings:SetCollider("Bad", { from = "post", radius = 0.1 }) end), "a collider with neither a 'to' nor an 'away' bone is refused")
		oCheck:Expect(not oSprings:SetGroupChains("Nope", { "rope_1" }), "chains for no group are refused")
		oSprings:SetGroupChains("Rope", { "nope" })
		tSpring.nPhase = 6
	elseif tSpring.nPhase == 6 then
		oCheck:Expect(Mentions(oSprings:GetError(), "'nope'"), "an unknown bone is named: " .. tostring(oSprings:GetError()))
		oSprings:SetGroupChains("Rope", { "rope_3" })
		tSpring.nPhase = 7
	elseif tSpring.nPhase == 7 then
		oCheck:Expect(Mentions(oSprings:GetError(), "single bone"), "a chain of one bone is refused: " .. tostring(oSprings:GetError()))
		tSpring.nPhase = 8
	end
end

function AnimationConstraintsCheck:OnUpdate(nDt)
	nFrame = nFrame + 1
	if bDone or nFrame < 2 then
		return
	end

	StepLook(nDt)
	StepSpring(nDt)

	if tLook.nPhase == 10 and tSpring.nPhase == 8 then
		bDone = true
		oCheck:Finish()
	end
end

return AnimationConstraintsCheck
