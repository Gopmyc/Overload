-- Levels of detail on model renderers: a rock whose levels come from the scene file, two more set from Lua with a
-- bias, spheres at three finenesses written by Tools/make_lods.py. The level against the camera's distance, both ways,
-- with the margin; the distance taken at the centre of the bounds, not the pivot; levels refused, and bones that must
-- match. Its first frames hold a view of the three rocks at their three levels for a screenshot (--frames 20 --screenshot)
local Check = require("Scripts/Checks/Check")

local ModelLodCheck = {}
local oCheck = Check.New("ModelLod")
local nFrame = 0
local t = {}

local SPHERE = { "Models/Lod/Sphere0.obj", "Models/Lod/Sphere1.obj", "Models/Lod/Sphere2.obj" }

local function Fails(fCall)
	local bOk, sError = pcall(fCall)
	return not bOk, sError
end

local function Mentions(sText, sPart)
	return type(sText) == "string" and sText:find(sPart, 1, true) ~= nil
end

local function MakeRock(sName, vPosition, sModel)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vPosition)
	local oRenderer = oActor:AddModelRenderer()
	oRenderer:SetModel(Resources.GetModel(sModel))
	oActor:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))
	return oRenderer
end

local function PlaceCamera(x, y, z)
	t.oCamera:GetTransform():SetLocalPosition(Vector3.new(x, y, z))
end

function ModelLodCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	t.oCamera = oScene:CreateActor("Camera", "")
	t.oCamera:AddCamera()
	PlaceCamera(0, 1, -6)

	local oSun = oScene:CreateActor("Sun", "")
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(40, -30, 0)))
	oSun:AddDirectionalLight():SetIntensity(3)
	local oAmbient = oScene:CreateActor("Ambient", ""):AddAmbientSphereLight()
	oAmbient:SetRadius(500)
	oAmbient:SetIntensity(0.15)

	local oRock = oScene:FindActorByName("Rock")
	t.oA = oRock and oRock:GetModelRenderer()

	-- The same levels, from Lua, with a bias that brings them nearer: from the first view, B at its first lighter level
	-- and C at its last
	local tLevels = { { Resources.GetModel(SPHERE[2]), 8 }, { Resources.GetModel(SPHERE[3]), 20 } }
	t.oB = MakeRock("RockB", Vector3.new(-3, 1, 0), SPHERE[1])
	t.oB:SetLodModels(tLevels)
	t.oB:SetLodBias(0.5)
	t.oC = MakeRock("RockC", Vector3.new(3, 1, 0), SPHERE[1])
	t.oC:SetLodModels(tLevels)
	t.oC:SetLodBias(0.25)

	-- A model whose geometry stands 40 m along +Z from its pivot, out of the first view
	t.oD = MakeRock("RockD", Vector3.new(30, 1, -40), "Models/Lod/Offset0.obj")
	t.oD:SetLodModels({ { Resources.GetModel(SPHERE[3]), 10 } })
end

function ModelLodCheck:OnUpdate()
	nFrame = nFrame + 1

	if nFrame == 25 then
		if not oCheck:Expect(t.oA ~= nil, "the scene's rock has its model renderer") then
			oCheck:Finish()
			return
		end

		-- The levels from the file
		local tModels = t.oA:GetLodModels()
		oCheck:Equal(#tModels, 2, "the rock has its two levels from the file")
		oCheck:Near(tModels[1] and tModels[1][2] or -1, 8, "the first starts at 8 m")
		oCheck:Near(tModels[2] and tModels[2][2] or -1, 20, "the second at 20 m")
		oCheck:Expect(tModels[1] and tModels[1][1] == Resources.GetModel(SPHERE[2]), "the first is the lighter sphere")
		oCheck:Near(t.oA:GetLodBias(), 1, "its bias from the file")
		oCheck:Equal(t.oA:GetLodError(), nil, "every level is drawn")

		-- From the first view, 6 to 7 m away: one rock at each level
		oCheck:Equal(t.oA:GetLodLevel(), 0, "the near rock at full detail")
		oCheck:Equal(t.oB:GetLodLevel(), 1, "a bias of 0.5 brings the first level to 4 m")
		oCheck:Equal(t.oC:GetLodLevel(), 2, "a bias of 0.25 brings the last level to 5 m")
		oCheck:Equal(t.oD:GetLodLevel(), 1, "the far rock at its lighter level")

		PlaceCamera(0, 1, -15)
	elseif nFrame == 27 then
		oCheck:Equal(t.oA:GetLodLevel(), 1, "at 15 m, the first lighter level")
		PlaceCamera(0, 1, -40)
	elseif nFrame == 29 then
		oCheck:Equal(t.oA:GetLodLevel(), 2, "at 40 m, the last level")
		PlaceCamera(0, 1, -5)
	elseif nFrame == 31 then
		oCheck:Equal(t.oA:GetLodLevel(), 0, "back at 5 m, full detail at once")
		PlaceCamera(0, 1, -8.5)
	elseif nFrame == 33 then
		oCheck:Equal(t.oA:GetLodLevel(), 0, "just past 8 m, within the margin, the level stays")
		PlaceCamera(0, 1, -9)
	elseif nFrame == 35 then
		oCheck:Equal(t.oA:GetLodLevel(), 1, "past the margin, the level changes")
		PlaceCamera(0, 1, -7.5)
	elseif nFrame == 37 then
		oCheck:Equal(t.oA:GetLodLevel(), 1, "coming back within the margin, it stays")
		PlaceCamera(0, 1, -7)
	elseif nFrame == 39 then
		oCheck:Equal(t.oA:GetLodLevel(), 0, "and past it, full detail")

		-- 3 m from the far rock's geometry, 37 m from its pivot
		PlaceCamera(30, 1, -3)
	elseif nFrame == 41 then
		oCheck:Equal(t.oD:GetLodLevel(), 0, "the distance is taken at the geometry, not at the pivot 37 m away")
		PlaceCamera(30, 1, -37)
	elseif nFrame == 43 then
		oCheck:Equal(t.oD:GetLodLevel(), 1, "near the pivot, 37 m from the geometry, the lighter level")

		PlaceCamera(0, 1, -15)
		t.oA:SetLodBias(4)
	elseif nFrame == 45 then
		oCheck:Equal(t.oA:GetLodLevel(), 0, "a bias of 4 keeps the detail at 15 m")
		oCheck:Near(t.oA:GetLodBias(), 4, "the bias reads back")

		-- Refused levels, each naming why
		local s1, s2 = Resources.GetModel(SPHERE[2]), Resources.GetModel(SPHERE[3])
		local bFailed, sWhy = Fails(function() t.oB:SetLodModels({ { s1, 10 }, { s2, 5 } }) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "level 2 must start beyond"), "levels must start further and further: " .. tostring(sWhy))
		bFailed, sWhy = Fails(function() t.oB:SetLodModels({ { nil, 10 } }) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "level 1 has no model"), "a level needs a model: " .. tostring(sWhy))
		bFailed, sWhy = Fails(function() t.oB:SetLodModels({ s1 }) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "not a table"), "a level is a table: " .. tostring(sWhy))
		local tMany = {}
		for i = 1, 8 do tMany[i] = { s1, i * 10 } end
		bFailed, sWhy = Fails(function() t.oB:SetLodModels(tMany) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "at most 7"), "at most 7 levels: " .. tostring(sWhy))
		oCheck:Expect(Fails(function() t.oB:SetLodBias(0) end), "a bias of 0 is refused")
		oCheck:Equal(#t.oB:GetLodModels(), 2, "a refused call leaves the levels as they were")

		-- Bones: a model with bones takes levels with the same bones
		local oMannequin = Scenes.GetCurrentScene():CreateActor("Mannequin", ""):AddModelRenderer()
		local oBody = Resources.GetModel(":Models/Mannequin.fbx")
		oMannequin:SetModel(oBody)
		bFailed, sWhy = Fails(function() oMannequin:SetLodModels({ { Resources.GetModel("Models/Rigs/Chain.gltf"), 10 } }) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "cannot take the model's pose"), "a rig with other bones is refused: " .. tostring(sWhy))
		bFailed, sWhy = Fails(function() oMannequin:SetLodModels({ { oBody, 10 } }) end)
		oCheck:Expect(not bFailed and oMannequin:GetLodError() == nil, "the same bones are taken: " .. tostring(sWhy))

		-- A level set before the model changes is left out, and the renderer says why
		local oLate = MakeRock("RockE", Vector3.new(0, -50, 0), SPHERE[1])
		oLate:SetLodModels({ { s1, 5 } })
		oLate:SetModel(oBody)
		oCheck:Expect(Mentions(oLate:GetLodError(), "level 1 is left out: it has no bones"), "a level without the model's bones is left out: " .. tostring(oLate:GetLodError()))

		-- No level: the model alone
		t.oC:SetLodModels({})
		oCheck:Equal(#t.oC:GetLodModels(), 0, "an empty list clears the levels")
	elseif nFrame == 47 then
		oCheck:Equal(t.oC:GetLodLevel(), 0, "without levels, the model itself")
		oCheck:Finish()
	end
end

return ModelLodCheck
