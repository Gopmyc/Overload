-- Vegetation over the Terrain check's hills: grass and stones from the scene file, scattered chunk by chunk and drawn
-- instanced. The layers read back, instances within reach and none from far away, the same scatter for the same seed,
-- a density map that empties or halves a layer, the height and slope filters, a material that cannot draw instances,
-- refused settings, and a layer removed. Its first frames hold a view for a screenshot (--frames 20 --screenshot)
local Check = require("Scripts/Checks/Check")

local VegetationCheck = {}
local oCheck = Check.New("Vegetation")
local nFrame = 0
local t = {}

local function Fails(fCall)
	local bOk, sError = pcall(fCall)
	return not bOk, sError
end

local function Mentions(sText, sPart)
	return type(sText) == "string" and sText:find(sPart, 1, true) ~= nil
end

local function PlaceCamera(x, y, z)
	t.oCamera:GetTransform():SetLocalPosition(Vector3.new(x, y, z))
end

function VegetationCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	t.oCamera = oScene:CreateActor("Camera", "")
	t.oCamera:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(18, 0, 0)))
	t.oCamera:AddCamera()
	PlaceCamera(0, 14, -52)

	local oSun = oScene:CreateActor("Sun", "")
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(40, -40, 0)))
	oSun:AddDirectionalLight():SetIntensity(4)
	local oAmbient = oScene:CreateActor("Ambient", ""):AddAmbientSphereLight()
	oAmbient:SetRadius(500)
	oAmbient:SetIntensity(0.1)

	local oHills = oScene:FindActorByName("Hills")
	t.oVegetation = oHills and oHills:GetVegetation()
end

function VegetationCheck:OnUpdate()
	nFrame = nFrame + 1
	local oVegetation = t.oVegetation

	if nFrame == 3 then
		if not oCheck:Expect(oVegetation ~= nil, "the scene's hills have their vegetation") then
			oCheck:Finish()
			return
		end

		-- The layers from the file
		oCheck:Equal(oVegetation:GetError(), nil, "every layer can be drawn: " .. tostring(oVegetation:GetError()))
		oCheck:Equal(oVegetation:GetLayerCount(), 2, "two layers")
		local tGrass = oVegetation:GetLayer(0) or {}
		oCheck:Equal(tGrass.name, "Grass", "the first is the grass")
		oCheck:Near(tGrass.density or -1, 1.5, "its density", 1e-5)
		oCheck:Near(tGrass.maxSlope or -1, 35, "its largest slope", 1e-5)
		oCheck:Equal(tGrass.seed, 7, "its seed")
		oCheck:Equal(tGrass.castShadows, false, "it casts no shadow")
		oCheck:Expect(tGrass.model == Resources.GetModel(":Models/Cone.fbx"), "it scatters the engine's cone")
		oCheck:Equal((oVegetation:GetLayer(1) or {}).castShadows, true, "the stones cast shadows")
		oCheck:Equal(oVegetation:GetLayer(2), nil, "no third layer")
	elseif nFrame == 25 then
		-- Within reach of the first view
		t.nGrass = oVegetation:GetInstanceCount(0)
		t.nStones = oVegetation:GetInstanceCount(1)
		oCheck:Expect(t.nGrass > 500, "grass within reach: " .. t.nGrass)
		oCheck:Expect(t.nStones > 0 and t.nStones < t.nGrass, "fewer stones: " .. t.nStones)
		oCheck:Equal(oVegetation:GetInstanceCount(), t.nGrass + t.nStones, "the count of every layer is the sum")

		-- The same seed scatters the same
		oVegetation:SetLayer(0, { seed = 7 })
	elseif nFrame == 27 then
		oCheck:Equal(oVegetation:GetInstanceCount(0), t.nGrass, "scattered again with the same seed, the same grass")

		PlaceCamera(0, 3000, 0)
	elseif nFrame == 29 then
		oCheck:Equal(oVegetation:GetInstanceCount(), 0, "from far away, no chunk within reach")

		PlaceCamera(0, 14, -52)
		local bSet = oVegetation:SetDensityMap(0, 2, { 0, 0, 0, 0 })
		oCheck:Expect(bSet, "a density map of zeros is taken")
	elseif nFrame == 31 then
		oCheck:Equal(oVegetation:GetInstanceCount(0), 0, "a density map of zeros empties the layer")
		oCheck:Expect(oVegetation:GetInstanceCount(1) == t.nStones, "and leaves the other one")

		-- Weights fading from the near edge (1) to the far edge (0)
		oVegetation:SetDensityMap(0, 2, { 0, 0, 1, 1 })
	elseif nFrame == 33 then
		local nFading = oVegetation:GetInstanceCount(0)
		oCheck:Expect(nFading > 0 and nFading < t.nGrass, "fading weights thin the grass: " .. nFading .. " of " .. t.nGrass)

		oCheck:Expect(oVegetation:SetDensityMap(0, 0, nil), "a size of 0 clears the map")
		oVegetation:SetLayer(0, { maxHeight = -1 })
	elseif nFrame == 35 then
		oCheck:Equal(oVegetation:GetInstanceCount(0), 0, "nothing grows below the lowest ground")

		oVegetation:SetLayer(0, { maxHeight = 7.5, maxSlope = 2 })
	elseif nFrame == 37 then
		local nFlat = oVegetation:GetInstanceCount(0)
		oCheck:Expect(nFlat < t.nGrass / 4, "on slopes under 2 degrees only, little grass: " .. nFlat)

		oVegetation:SetLayer(0, { maxSlope = 35 })
	elseif nFrame == 39 then
		oCheck:Equal(oVegetation:GetInstanceCount(0), t.nGrass, "its settings back, the same grass")

		-- Refused settings, each naming why
		local bFailed, sWhy = Fails(function() oVegetation:AddLayer({ densty = 1 }) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "unknown setting 'densty'"), "an unknown setting is refused: " .. tostring(sWhy))
		bFailed, sWhy = Fails(function() oVegetation:AddLayer({ density = 500 }) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "'density' must be a number from 0 to 100"), "too dense: " .. tostring(sWhy))
		bFailed, sWhy = Fails(function() oVegetation:SetLayer(0, { castShadows = 1 }) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "'castShadows' must be true or false"), "a value of the wrong kind: " .. tostring(sWhy))
		bFailed, sWhy = Fails(function() oVegetation:GetInstanceCount(5) end)
		oCheck:Expect(bFailed and Mentions(sWhy, "no layer 5"), "an unknown layer: " .. tostring(sWhy))
		local bSet, sWhyMap = oVegetation:SetDensityMap(0, 3, { 1 })
		oCheck:Expect(not bSet and Mentions(sWhyMap, "9 expected"), "a density map of the wrong size: " .. tostring(sWhyMap))
		oCheck:Equal(oVegetation:GetLayerCount(), 2, "a refused call adds no layer")

		-- A material whose shader cannot draw instances
		local nUnlit = oVegetation:AddLayer({ model = Resources.GetModel(":Models/Cone.fbx"), material = Resources.GetMaterial("Materials/UnlitFog.ovmat") })
		oCheck:Equal(nUnlit, 2, "a third layer")
		oCheck:Expect(Mentions(oVegetation:GetError(), "INSTANCING"), "an unlit material cannot draw instances: " .. tostring(oVegetation:GetError()))
		oVegetation:RemoveLayer(2)
		oCheck:Equal(oVegetation:GetError(), nil, "removed, the error goes")

		oVegetation:RemoveLayer(1)
	elseif nFrame == 41 then
		oCheck:Equal(oVegetation:GetLayerCount(), 1, "the stones removed")
		oCheck:Equal(oVegetation:GetInstanceCount(), t.nGrass, "only the grass is drawn")
		oCheck:Finish()
	end
end

return VegetationCheck
