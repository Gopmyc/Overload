-- Navigation from Lua: a navigation mesh built from the scene's static colliders, paths round a wall through its gap
-- and up a ramp, the nearest point, a walk stopped by the wall, a door that shuts and opens again with Rebuild, seeds,
-- and refused arguments. It also draws, for a screenshot (--frames 20 --screenshot), the mesh over the floor and a
-- path's corners
local Check = require("Scripts/Checks/Check")

local NavigationCheck = {}
local oCheck = Check.New("Navigation")
local nFrame = 0
local t = {}

local function Fails(fCall)
	local bOk = pcall(fCall)
	return not bOk
end

-- A static box collider, drawn as a cube of its size
local function Block(sName, vCenter, vHalf, qRotation)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vCenter)
	if qRotation then oActor:GetTransform():SetLocalRotation(qRotation) end
	oActor:GetTransform():SetLocalScale(vHalf * 2)

	local oBody = oActor:AddPhysicalBox()
	oBody:SetSize(Vector3.new(0.5, 0.5, 0.5))
	oBody:SetStatic(true)

	oActor:AddModelRenderer():SetModel(Resources.GetModel(":Models/Cube.fbx"))
	oActor:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial("Materials/Ground.ovmat"))
	return oActor
end

-- The x at which a path crosses z = 0
local function CrossingX(tCorners)
	for i = 2, #tCorners do
		local a, b = tCorners[i - 1], tCorners[i]
		if (a.z < 0) ~= (b.z < 0) then
			return a.x + (b.x - a.x) * (-a.z / (b.z - a.z))
		end
	end
end

function NavigationCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:GetTransform():SetLocalPosition(Vector3.new(0, 16, -14))
	oCamera:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(50, 0, 0)))
	oCamera:AddCamera()

	local oSun = oScene:CreateActor("Sun", "")
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(50, -30, 0)))
	oSun:AddDirectionalLight()
	local oAmbient = oScene:CreateActor("Ambient", ""):AddAmbientSphereLight()
	oAmbient:SetRadius(100)
	oAmbient:SetIntensity(0.5)

	-- A floor, a wall across it with a gap from x = 3 to x = 5, and a platform 1.5 m high with a ramp up to it
	Block("Floor", Vector3.new(0, -0.5, 0), Vector3.new(10, 0.5, 10))
	Block("Wall West", Vector3.new(-3.5, 1, 0), Vector3.new(6.5, 1, 0.25))
	Block("Wall East", Vector3.new(7.5, 1, 0), Vector3.new(2.5, 1, 0.25))
	Block("Platform", Vector3.new(-6, 0.75, 6), Vector3.new(2, 0.75, 2))
	-- 20° up towards -X: its top runs from the floor at x = 0.1 to the platform's edge at x = -4
	Block("Ramp", Vector3.new(-1.9, 0.5, 6), Vector3.new(2.3, 0.1, 1), Quaternion.new(Vector3.new(0, 0, -20)))
end

function NavigationCheck:OnUpdate()
	nFrame = nFrame + 1

	if nFrame == 2 then
		oCheck:Expect(not Navigation.IsBuilt(), "nothing is built at first")
		local bBuilt, sError = Navigation.Build()
		oCheck:Expect(bBuilt, "Build: " .. tostring(sError))
		oCheck:Expect(Navigation.IsBuilt() and Navigation.GetPolygonCount() > 0, "built, with polygons")
		local tSettings = Navigation.GetSettings()
		oCheck:Expect(math.abs(tSettings.agentRadius - 0.4) < 1e-6 and tSettings.tileSize == 48 and tSettings.layerMask == 65535, "the default settings")

		-- Round the wall, through the gap, the agent's radius from its sides
		local tCorners, bComplete = Navigation.FindPath(Vector3.new(0, 0, -5), Vector3.new(0, 0, 5))
		oCheck:Expect(tCorners ~= nil and bComplete, "a complete path round the wall")
		if tCorners then
			local nCrossing = CrossingX(tCorners)
			oCheck:Expect(nCrossing ~= nil and nCrossing > 3.2 and nCrossing < 4.8, "through the gap: " .. tostring(nCrossing))
			oCheck:Expect(math.abs(tCorners[#tCorners].z - 5) < 0.1, "to the end")
			t.tCorners = tCorners
		end

		-- Up the ramp onto the platform
		local tUp, bUp = Navigation.FindPath(Vector3.new(0, 0, 3), Vector3.new(-6, 1.5, 6))
		oCheck:Expect(tUp ~= nil and bUp and math.abs(tUp[#tUp].y - 1.5) < 0.3, "up the ramp onto the platform")

		-- The nearest point on the floor, a walk stopped by the wall, one through the gap
		local vNearest = Navigation.FindNearestPoint(Vector3.new(-4, 1.5, -5))
		oCheck:Expect(vNearest ~= nil and math.abs(vNearest.y) < 0.3, "the nearest point is on the floor")
		oCheck:Expect(Navigation.FindNearestPoint(Vector3.new(0, 40, 0)) == nil, "none far above")
		local bHit, vStop = Navigation.Raycast(Vector3.new(-2, 0, -5), Vector3.new(-2, 0, 5))
		oCheck:Expect(bHit == true and vStop.z < -0.2, "a walk stopped by the wall")
		local bThrough = Navigation.Raycast(Vector3.new(4, 0, -5), Vector3.new(4, 0, 5))
		oCheck:Expect(bThrough == false, "a walk through the gap")

		-- Random points: on the mesh, the same for a seed, within a radius
		Navigation.SetSeed(3)
		local vFirst = Navigation.GetRandomPoint()
		Navigation.SetSeed(3)
		local vAgain = Navigation.GetRandomPoint()
		oCheck:Expect(vFirst ~= nil and vAgain ~= nil and vFirst.x == vAgain.x and vFirst.z == vAgain.z, "the same random point for a seed")
		local vAround = Navigation.GetRandomPoint(Vector3.new(-5, 0, -5), 1.5)
		oCheck:Expect(vAround ~= nil and (vAround - Vector3.new(-5, vAround.y, -5)):Length() <= 1.5001, "a random point within a radius")

		-- A door shuts the gap
		t.oDoor = Block("Door", Vector3.new(4, 1, 0), Vector3.new(1, 1, 0.25))
		local bRebuilt = Navigation.Rebuild(Vector3.new(3, 0, -0.25), Vector3.new(5, 2, 0.25))
		local _, bOpen = Navigation.FindPath(Vector3.new(0, 0, -5), Vector3.new(0, 0, 5))
		oCheck:Expect(bRebuilt and bOpen == false, "a shut door leaves only a path that stops short")
		t.oDoor:Destroy()
	elseif nFrame == 3 then
		-- The door freed at the end of the frame before: the gap opens again
		Navigation.Rebuild(Vector3.new(3, 0, -0.25), Vector3.new(5, 2, 0.25))
		local _, bOpen = Navigation.FindPath(Vector3.new(0, 0, -5), Vector3.new(0, 0, 5))
		oCheck:Expect(bOpen == true, "the door gone, the path goes through again")

		oCheck:Expect(Fails(function() Navigation.Build({ agentRadious = 1 }) end), "an unknown setting is refused")
		oCheck:Expect(Fails(function() Navigation.Build({ tileSize = 1.5 }) end), "a tile size that is not whole is refused")
		oCheck:Expect(Fails(function() Navigation.GetRandomPoint(Vector3.new(0, 0, 0)) end), "a center without a radius is refused")
		oCheck:Expect(Fails(function() Navigation.SetSeed(-1) end), "a negative seed is refused")

		-- No layer, no geometry: the mesh built before is gone
		local bBuilt, sError = Navigation.Build({ layerMask = 0 })
		oCheck:Expect(not bBuilt and sError ~= nil and sError:find("no geometry") ~= nil, "no layer, no geometry: " .. tostring(sError))
		local tNone, sWhy = Navigation.FindPath(Vector3.new(0, 0, -5), Vector3.new(0, 0, 5))
		oCheck:Expect(tNone == nil and type(sWhy) == "string", "no mesh, no path")

		-- Built again, and drawn: the mesh just over the floor, and the path's corners
		Navigation.Build()
		local oModel = Resources.CreateModel("Generated/NavigationMesh", Navigation.GetDebugGeometry(), "Overlay")
		local oOverlay = Scenes.GetCurrentScene():CreateActor("Navigation Mesh", "")
		oOverlay:GetTransform():SetLocalPosition(Vector3.new(0, 0.03, 0))
		oOverlay:AddModelRenderer():SetModel(oModel)
		oOverlay:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial("Materials/NavOverlay.ovmat"))

		for i, vCorner in ipairs(t.tCorners or {}) do
			local oMark = Scenes.GetCurrentScene():CreateActor("Corner " .. i, "")
			oMark:GetTransform():SetLocalPosition(vCorner + Vector3.new(0, 0.2, 0))
			oMark:GetTransform():SetLocalScale(Vector3.new(0.4, 0.4, 0.4))
			oMark:AddModelRenderer():SetModel(Resources.GetModel(":Models/Sphere.fbx"))
			oMark:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial("Materials/Standard.ovmat"))
		end
	elseif nFrame == 30 then
		oCheck:Finish()
	end
end

return NavigationCheck
