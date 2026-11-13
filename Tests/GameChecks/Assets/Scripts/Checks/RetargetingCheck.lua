-- Clips on other rigs, on the rigs of Models/Rigs (Tests/GameChecks/Tools/make_rigs.py): Chain's clip played on a rig
-- whose bones have other names, through a bone map, and on a rig whose rests turn otherwise, each bone checked against
-- Chain's own at three times; a rig scaled otherwise and one whose bones hang otherwise refused, naming the bone. It
-- also draws the five, for a screenshot (--frames 16 --screenshot)
local Check = require("Scripts/Checks/Check")

local RetargetingCheck = {}
local oCheck = Check.New("Retargeting")
local nFrame = 0
local t = {}

local RIGS = { "Chain", "ChainRenamed", "ChainTurned", "ChainScaled", "ChainReparented" }

local function Fails(fCall)
	local bOk = pcall(fCall)
	return not bOk
end

local function Mentions(sText, sPart)
	return type(sText) == "string" and sText:find(sPart, 1, true) ~= nil
end

local function SameRotation(qA, qB)
	return math.abs(math.abs(qA.x * qB.x + qA.y * qB.y + qA.z * qB.z + qA.w * qB.w) - 1) < 1e-4
end

local function Conjugate(q)
	return Quaternion.new(-q.x, -q.y, -q.z, q.w)
end

function RetargetingCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:GetTransform():SetLocalPosition(Vector3.new(0, 1.5, -7))
	oCamera:AddCamera()
	oScene:CreateActor("Sun", ""):AddDirectionalLight()
	oScene:CreateActor("Ambient", ""):AddAmbientSphereLight():SetRadius(100)

	for i, sName in ipairs(RIGS) do
		local oActor = oScene:CreateActor(sName, "")
		oActor:GetTransform():SetLocalPosition(Vector3.new((i - 3) * 1.5, 0, 0))
		oActor:AddModelRenderer():SetModel(Resources.GetModel("Models/Rigs/" .. sName .. ".gltf"))
		oActor:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))
		t[sName] = oActor:AddSkinnedMeshRenderer()
	end
end

function RetargetingCheck:OnUpdate()
	nFrame = nFrame + 1

	if nFrame == 2 then
		local oChain = Resources.GetModel("Models/Rigs/Chain.gltf")

		-- The rests, before any clip plays
		t.tChainRest, t.tTurnedRest = {}, {}
		for i = 0, t.Chain:GetBoneCount() - 1 do
			t.tChainRest[i] = t.Chain:GetBoneGlobalRotation(i)
			t.tTurnedRest[i] = t.ChainTurned:GetBoneGlobalRotation(i)
		end

		-- Other names: refused, then matched through the bone map
		t.ChainRenamed:SetAnimationSourceModel(oChain)
		oCheck:Expect(not t.ChainRenamed:IsAnimationSourceCompatible(), "bones of other names are refused without a map")
		oCheck:Expect(Mentions(t.ChainRenamed:GetAnimationSourceError(), "Hip"), "the refusal names the bone: " .. tostring(t.ChainRenamed:GetAnimationSourceError()))

		local tMap = { Hip = "root", Spine = "mid", Head = "tip" }
		t.ChainRenamed:SetAnimationBoneMap(tMap)
		oCheck:Expect(t.ChainRenamed:IsAnimationSourceCompatible(), "through the bone map, they are matched")
		oCheck:Equal(t.ChainRenamed:GetAnimationSourceError(), nil, "and nothing is wrong")
		local tRead = t.ChainRenamed:GetAnimationBoneMap()
		oCheck:Expect(tRead.Hip == "root" and tRead.Spine == "mid" and tRead.Head == "tip", "the bone map reads back")
		oCheck:Expect(Fails(function() t.ChainRenamed:SetAnimationBoneMap({ Hip = 3 }) end), "a map of other than names is refused")

		-- Rests turned otherwise: matched
		t.ChainTurned:SetAnimationSourceModel(oChain)
		oCheck:Expect(t.ChainTurned:IsAnimationSourceCompatible(), "rests turned otherwise are matched")

		-- Refused, naming the bone
		t.ChainScaled:SetAnimationSourceModel(oChain)
		oCheck:Expect(Mentions(t.ChainScaled:GetAnimationSourceError(), "'mid'"), "another rest scale is refused: " .. tostring(t.ChainScaled:GetAnimationSourceError()))
		t.ChainReparented:SetAnimationSourceModel(oChain)
		oCheck:Expect(Mentions(t.ChainReparented:GetAnimationSourceError(), "'tip'"), "another parent is refused: " .. tostring(t.ChainReparented:GetAnimationSourceError()))

		-- The same clip at the same times, bone by bone
		for _, sName in ipairs({ "Chain", "ChainRenamed", "ChainTurned" }) do
			oCheck:Expect(t[sName]:SetAnimation("Bend"), sName .. " plays Bend")
			t[sName]:Pause()
		end

		for _, nTime in ipairs({ 0.25, 0.5, 0.8 }) do
			for _, sName in ipairs({ "Chain", "ChainRenamed", "ChainTurned" }) do
				t[sName]:SetTime(nTime)
			end

			local bPositions, bTurns = true, true
			for i = 0, t.Chain:GetBoneCount() - 1 do
				local vChain = t.Chain:GetBoneGlobalPosition(i)
				bPositions = bPositions
					and Vector3.Distance(vChain, t.ChainRenamed:GetBoneGlobalPosition(i)) < 1e-4
					and Vector3.Distance(vChain, t.ChainTurned:GetBoneGlobalPosition(i)) < 1e-4

				-- Each turned bone turns away from its own rest as Chain's does from its own
				local qChainTurn = t.Chain:GetBoneGlobalRotation(i) * Conjugate(t.tChainRest[i])
				local qTurnedTurn = t.ChainTurned:GetBoneGlobalRotation(i) * Conjugate(t.tTurnedRest[i])
				bTurns = bTurns and SameRotation(qChainTurn, qTurnedTurn)
			end
			oCheck:Expect(bPositions, "at " .. nTime .. " s, every joint stands where Chain's does")
			oCheck:Expect(bTurns, "at " .. nTime .. " s, every turned bone turns as Chain's")
		end

		-- And the tip has moved from its rest, so that the comparison means something
		oCheck:Expect(Vector3.Distance(t.Chain:GetBoneGlobalPosition(2), Vector3.new(0, 2, 0)) > 0.3, "the clip moves the tip")

		for _, sName in ipairs({ "Chain", "ChainRenamed", "ChainTurned" }) do
			t[sName]:SetTime(0.5)
		end
	elseif nFrame == 20 then
		oCheck:Finish()
	end
end

return RetargetingCheck
