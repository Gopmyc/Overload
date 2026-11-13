-- A terrain from the scene file: a height field made from a 16-bit height map (Textures/Terrain/Hills.png, written by
-- Tools/make_heightmap.py) and drawn by a Terrain with its colour map. The heights against the map's formula and the
-- physics, the levels of detail as the camera moves away and back, a navigation mesh on the hills, heights a script
-- gives, and refused arguments. Its first frames hold the view of a screenshot (--frames 20 --screenshot)
local Check = require("Scripts/Checks/Check")

local TerrainCheck = {}
local oCheck = Check.New("Terrain")
local nFrame = 0
local t = {}

local SIZE = 129
local SCALE = 12

-- The height map's formula, from 0 to 1 (Tools/make_heightmap.py)
local function MapHeight(x, z)
	local nValue = 0.5
	nValue = nValue + 0.22 * math.sin(x * 2 * math.pi / 64) * math.cos(z * 2 * math.pi / 80)
	nValue = nValue + 0.12 * math.sin((x + z) * 2 * math.pi / 48)
	return math.min(math.max(nValue, 0), 1)
end

local function Fails(fCall)
	return not pcall(fCall)
end

local function Mentions(sText, sPart)
	return type(sText) == "string" and sText:find(sPart, 1, true) ~= nil
end

local function GroundBelow(x, z)
	local tHits = Physics.CastRay(Vector3.new(x, 100, z), Vector3.new(0, -1, 0), 200)
	return tHits[1] and tHits[1].Point.y or nil
end

function TerrainCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	t.oCamera = oScene:CreateActor("Camera", "")
	t.oCamera:GetTransform():SetLocalPosition(Vector3.new(0, 45, -105))
	t.oCamera:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(28, 0, 0)))
	t.oCamera:AddCamera()

	local oSun = oScene:CreateActor("Sun", "")
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(40, -40, 0)))
	oSun:AddDirectionalLight():SetIntensity(4)
	local oAmbient = oScene:CreateActor("Ambient", ""):AddAmbientSphereLight()
	oAmbient:SetRadius(500)
	oAmbient:SetIntensity(0.1)

	t.oHills = oScene:FindActorByName("Hills")
end

function TerrainCheck:OnUpdate()
	nFrame = nFrame + 1

	if nFrame == 3 then
		local oField = t.oHills and t.oHills:GetPhysicalHeightField()
		local oTerrain = t.oHills and t.oHills:GetTerrain()
		t.oField, t.oTerrain = oField, oTerrain
		if not oCheck:Expect(oField ~= nil and oTerrain ~= nil, "the scene's terrain has its height field and its Terrain") then
			oCheck:Finish()
			return
		end

		-- Made from the height map named in the scene file
		oCheck:Equal(oField:GetHeightmap(), "Textures/Terrain/Hills.png", "the height field names its height map")
		oCheck:Equal(oField:GetHeightmapError(), nil, "which reads")
		oCheck:Equal(oField:GetSampleCount(), SIZE, "a sample per pixel")
		oCheck:Near(oField:GetSpacing(), 1, "the spacing from the file", 1e-6)
		oCheck:Near(oField:GetHeightScale(), SCALE, "the scale from the file", 1e-6)
		oCheck:Equal(#oField:GetHeights(), SIZE * SIZE, "every height reads back")
		oCheck:Equal(oTerrain:GetError(), nil, "the terrain has all it needs: " .. tostring(oTerrain:GetError()))
		oCheck:Equal(oTerrain:GetChunkCount(), 16, "128 cells a side, in chunks of 32")
		oCheck:Equal(oTerrain:GetColorMap(), "Textures/Terrain/HillsColor.png", "and its colour map")

		-- The heights are the map's, on every sample tried, 16 bits keeping them within a millimetre
		local nWorst = 0
		for _, tSample in ipairs({ { 0, 0 }, { 128, 128 }, { 17, 93 }, { 64, 64 }, { 101, 5 }, { 40, 77 } }) do
			local x, z = tSample[1], tSample[2]
			local nHeight = oField:GetHeightAt(x - 64, z - 64)
			nWorst = math.max(nWorst, math.abs((nHeight or math.huge) - MapHeight(x, z) * SCALE))
		end
		oCheck:Expect(nWorst < 1e-3, "each sample stands at its pixel's height: worst " .. string.format("%.5f", nWorst))

		-- Between samples, on the triangles the physics uses
		local nWorstRay = 0
		for _, tPoint in ipairs({ { 0.3, 0.6 }, { -40.7, 22.2 }, { 50.5, -61.25 }, { -12.9, -12.1 }, { 33.3, 44.4 } }) do
			local nHeight = oField:GetHeightAt(tPoint[1], tPoint[2])
			local nGround = GroundBelow(tPoint[1], tPoint[2])
			nWorstRay = math.max(nWorstRay, math.abs((nHeight or math.huge) - (nGround or -math.huge)))
		end
		oCheck:Expect(nWorstRay < 0.02, "the height under a point is where a ray meets the ground: worst " .. string.format("%.4f", nWorstRay))
		oCheck:Equal(oField:GetHeightAt(70, 0), nil, "beyond the field, no height")
		local vNormal = oField:GetNormalAt(0, 0)
		oCheck:Expect(vNormal ~= nil and vNormal.y > 0.5 and math.abs(Vector3.Length(vNormal) - 1) < 1e-4, "the normal points up the slope")

		-- Refused arguments
		local bSet, sWhy = oTerrain:SetColors(2, { 1, 0 })
		oCheck:Expect(not bSet and Mentions(sWhy, "3 per sample"), "colours of the wrong count are refused: " .. tostring(sWhy))
		oCheck:Expect(Fails(function() oTerrain:SetChunkSize(0) end), "a chunk of no cell is refused")
		oCheck:Expect(Fails(function() oTerrain:SetLodCount(5) end), "more than 4 levels are refused")
		oCheck:Expect(Fails(function() oField:SetSpacing(0) end), "a spacing of 0 is refused")

		-- A navigation mesh over the hills, a path across them
		local bBuilt, sError = Navigation.Build()
		oCheck:Expect(bBuilt, "a navigation mesh is built on the terrain: " .. tostring(sError))
		local tCorners, bComplete = Navigation.FindPath(Vector3.new(-50, MapHeight(14, 14) * SCALE, -50), Vector3.new(-20, MapHeight(44, 24) * SCALE, -40))
		oCheck:Expect(tCorners ~= nil and bComplete, "a path crosses the hills")
		local nOff = 0
		for _, vCorner in ipairs(tCorners or {}) do
			nOff = math.max(nOff, math.abs(vCorner.y - (oField:GetHeightAt(vCorner.x, vCorner.z) or math.huge)))
		end
		oCheck:Expect(nOff < 0.5, "its corners lie on the ground: worst " .. string.format("%.2f", nOff))
	elseif nFrame == 30 then
		-- The view of the screenshot: some chunks near, some far
		t.nNear = t.oTerrain:GetTriangleCount()
		oCheck:Expect(t.nNear > 512, "from the first view, nearer chunks keep more detail: " .. t.nNear .. " triangles")

		t.oCamera:GetTransform():SetLocalPosition(Vector3.new(0, 3000, 0))
	elseif nFrame == 33 then
		-- Far away, every chunk at its last level: 4 cells a side, 32 triangles
		local bAllLast = true
		for i = 0, t.oTerrain:GetChunkCount() - 1 do
			bAllLast = bAllLast and t.oTerrain:GetChunkLod(i) == 3
		end
		oCheck:Expect(bAllLast, "from far away, every chunk is at its last level")
		oCheck:Equal(t.oTerrain:GetTriangleCount(), 16 * 32, "and the terrain is 512 triangles")

		t.oCamera:GetTransform():SetLocalPosition(Vector3.new(0, 20, 0))
	elseif nFrame == 36 then
		-- Over the centre, the four chunks round it at full detail
		local bNear = true
		for _, nChunk in ipairs({ 5, 6, 9, 10 }) do
			bNear = bNear and t.oTerrain:GetChunkLod(nChunk) == 0
		end
		oCheck:Expect(bNear, "close above the centre, the chunks round it are at full detail")
		oCheck:Expect(t.oTerrain:GetTriangleCount() > t.nNear, "and the terrain draws more triangles: " .. t.oTerrain:GetTriangleCount())

		-- Heights a script gives take the field over, then the height map comes back
		local tFlat = {}
		for i = 1, 9 do tFlat[i] = 2 end
		local bSet = t.oField:SetHeights(3, 4, tFlat)
		oCheck:Expect(bSet, "a script sets its own heights")
		oCheck:Equal(t.oField:GetHeightmap(), "", "the field leaves its height map")
		oCheck:Equal(t.oTerrain:GetChunkCount(), 1, "the terrain follows: one chunk")
		oCheck:Near(t.oField:GetHeightAt(-60, -60) or -1, 2, "and the ground is flat", 1e-5)

		t.oField:SetHeightmap("Textures/Terrain/Hills.png")
		oCheck:Equal(t.oTerrain:GetChunkCount(), 16, "the height map makes the field again")
		oCheck:Equal(t.oTerrain:GetError(), nil, "and the terrain draws it")
	elseif nFrame == 38 then
		oCheck:Finish()
	end
end

return TerrainCheck
