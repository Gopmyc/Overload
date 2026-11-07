-- Physics queries from Lua, on a column of boxes: an enemy, a trigger, a wall, then a floor on the project's layers
local Check = Resources.GetScript("Scripts/Checks/Check.lua")

local QueriesCheck = {}
local oCheck = Check.New("Queries")
local nFrame = 0
local tBoxes = {}

local function Box(sName, nY, vHalfExtents, sLayer, bTrigger)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(Vector3.new(0, nY, 0))
	local oBody = oActor:AddPhysicalBox()
	oBody:SetSize(vHalfExtents)
	oBody:SetStatic(true)
	oBody:SetCollisionLayer(Physics.GetLayerIndex(sLayer))
	oBody:SetTrigger(bTrigger or false)
	tBoxes[sName] = oBody
end

local function Names(tObjects)
	local tNames = {}
	for nIndex, oObject in ipairs(tObjects) do
		tNames[nIndex] = (oObject.Object or oObject):GetOwner():GetName()
	end
	return table.concat(tNames, ",")
end

local function SortedNames(tObjects)
	local tNames = {}
	for nIndex, oObject in ipairs(tObjects) do
		tNames[nIndex] = oObject:GetOwner():GetName()
	end
	table.sort(tNames)
	return table.concat(tNames, ",")
end

function QueriesCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame == 2 then
		Box("Enemy", -5, Vector3.new(0.5, 0.5, 0.5), "Enemy")
		Box("Trigger", -10, Vector3.new(0.5, 0.5, 0.5), "Default", true)
		Box("Wall", -15, Vector3.new(0.5, 0.5, 0.5), "Default")
		Box("Floor", -20, Vector3.new(2, 0.5, 2), "Floor")
	elseif nFrame == 5 then
		local vFrom, vDown = Vector3.new(0, 0, 0), Vector3.new(0, -1, 0)

		local tHits = Physics.CastRay(vFrom, vDown, 30)
		oCheck:Equal(Names(tHits), "Enemy,Wall,Floor", "ray order")
		if #tHits == 3 then
			oCheck:Near(tHits[1].Distance, 4.5, "enemy distance")
			oCheck:Near(tHits[3].Distance, 19.5, "floor distance")
			oCheck:Near(tHits[3].Point.y, -19.5, "floor point")
			oCheck:Near(tHits[3].Normal.y, 1, "floor normal")
		end

		oCheck:Equal(Names(Physics.CastRay(vFrom, vDown, 30, nil, true)), "Enemy,Trigger,Wall,Floor", "ray with triggers")
		oCheck:Equal(Physics.GetLayerMask("Floor"), 8, "mask of a named layer")
		oCheck:Equal(Physics.GetLayerMask(0, "Enemy"), 5, "mask of a layer index and a name")
		oCheck:Equal(Names(Physics.CastRay(vFrom, vDown, 30, Physics.GetLayerMask("Floor"))), "Floor", "ray on the floor's layer")
		oCheck:Equal(Names(Physics.CastRay(vFrom, vDown, 3)), "", "ray too short")
		oCheck:Equal(Names(Physics.CastRay(vFrom, Vector3.new(0, 0, 0), 30)), "", "ray without direction")

		local tSide = Physics.CastRay(Vector3.new(-10, -15, 0), Vector3.new(1, 0, 0), 20)
		if oCheck:Equal(Names(tSide), "Wall", "sideways ray") then
			oCheck:Near(tSide[1].Normal.x, -1, "side normal")
		end

		local tSphere = Physics.CastSphere(vFrom, 0.25, vDown, 30)
		if oCheck:Equal(Names(tSphere), "Enemy,Wall,Floor", "sphere cast") then
			oCheck:Near(tSphere[1].Distance, 4.25, "sphere cast distance")
		end

		local vWall = Vector3.new(0, -15, 0)
		oCheck:Equal(SortedNames(Physics.OverlapSphere(vWall, 0.6)), "Wall", "small sphere overlap")
		oCheck:Equal(SortedNames(Physics.OverlapSphere(vWall, 6)), "Floor,Wall", "big sphere overlap")
		oCheck:Equal(SortedNames(Physics.OverlapSphere(vWall, 6, nil, true)), "Floor,Trigger,Wall", "big sphere overlap with triggers")
		oCheck:Equal(SortedNames(Physics.OverlapBox(Vector3.new(1.2, -15, 0), Vector3.new(0.8, 0.05, 0.05))), "Wall", "box overlap")
		oCheck:Equal(SortedNames(Physics.OverlapBox(Vector3.new(1.2, -15, 0), Vector3.new(0.8, 0.05, 0.05), Quaternion.new(Vector3.new(0, 90, 0)))), "", "turned box overlap")

		local oOld = Physics.Raycast(vFrom, vDown, 30)
		oCheck:Equal(oOld and #oOld.ResultObjects, 4, "former Raycast, triggers included")

		oCheck:Finish()
	end
end

return QueriesCheck
