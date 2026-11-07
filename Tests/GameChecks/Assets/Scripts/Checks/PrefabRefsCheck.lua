-- A reference between two actors of a prefab names, in each instance, that instance's own actor; a reference to an
-- actor outside the prefab is kept as written
local Check = require("Scripts/Checks/Check")

local PrefabRefsCheck = {}
local oCheck = Check.New("PrefabRefs")
local nFrame = 0

local function CheckInstance(oInstance, sWhich)
	if not oCheck:Expect(oInstance ~= nil, sWhich .. " instance made") then
		return nil
	end

	local oLink = oInstance:GetBehaviour("PairLink")
	if not oCheck:Expect(oLink ~= nil, sWhich .. " instance has its Behaviour") then
		return nil
	end

	local oTarget = oLink.oTarget
	if oCheck:Expect(oTarget ~= nil, sWhich .. " target resolved") then
		oCheck:Equal(oTarget:GetName(), "Target", sWhich .. " target name")
		oCheck:Equal(oTarget:GetParent() and oTarget:GetParent():GetGUID(), oInstance:GetGUID(), sWhich .. " target is the instance's own child")
	end

	local oOutside = oLink.oOutside
	if oCheck:Expect(oOutside ~= nil, sWhich .. " outside reference resolved") then
		oCheck:Equal(oOutside:GetName(), "Outside", sWhich .. " outside reference kept")
	end

	return oTarget and oTarget:GetGUID()
end

function PrefabRefsCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame == 2 then
		local oScene = Scenes.GetCurrentScene()
		local oPrefab = Prefab()
		oPrefab.path = "Prefabs/Pair.ovprefab"

		local nFirst = CheckInstance(oScene:InstantiatePrefab(oPrefab), "first")
		local nSecond = CheckInstance(oScene:InstantiatePrefab(oPrefab), "second")
		oCheck:Expect(nFirst ~= nil and nSecond ~= nil and nFirst ~= nSecond, "each instance has a target of its own")
		oCheck:Expect(nFirst ~= 7102 and nSecond ~= 7102, "the instances do not keep the prefab's GUIDs")
		oCheck:Finish()
	end
end

return PrefabRefsCheck
