-- Components as Lua holds them: handles checked on every use. A component removed, or of an actor freed, raises a
-- clear error instead of being read; handles compare by component; every way a component reaches Lua gives one
local Check = require("Scripts/Checks/Check")

local ComponentRefsCheck = {}
local oCheck = Check.New("ComponentRefs")
local nFrame = 0
local oDoomed, oDoomedTransform, oBody

local kGone = "attempt to use a component that was removed, or whose actor was destroyed"

-- Calls a function and returns the error it raised, or nil
local function ErrorOf(fCall)
	local bOk, sError = pcall(fCall)
	return not bOk and tostring(sError) or nil
end

local function IsGone(fCall)
	local sError = ErrorOf(fCall)
	return sError ~= nil and sError:find(kGone, 1, true) ~= nil
end

function ComponentRefsCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1
	local oScene = Scenes.GetCurrentScene()

	if nFrame == 2 then
		-- From the actor's getters and adders, the UI components included
		local oLamp = oScene:CreateActor("Lamp", "")
		local oTransform = oLamp:GetTransform()
		local oLight = oLamp:AddPointLight()
		local oImage = oLamp:AddImage()
		oCheck:Expect(oTransform:IsAlive() and oLight:IsAlive() and oImage:IsAlive(), "handles are alive while their components are there")
		oCheck:Expect(oLamp:GetTransform() == oTransform and oLamp:GetPointLight() == oLight and oLamp:GetLight() == oLight, "two handles on one component are equal")
		oCheck:Expect(oLamp:GetImage() ~= oLight, "handles on two components are not")

		-- Removed: a clear error, through a member function, a lambda and a base usertype's method
		oCheck:Expect(oLamp:RemovePointLight(), "the light is removed")
		oCheck:Expect(not oLight:IsAlive(), "a removed component's handle is not alive")
		oCheck:Expect(IsGone(function() oLight:SetIntensity(2) end), "a removed component raises when set")
		oCheck:Expect(IsGone(function() return oLight:GetOwner() end), "and when asked for its owner")
		oCheck:Expect(IsGone(function() return oLight:GetConstant() end), "and through its own usertype's methods")
		oCheck:Expect(oLamp:GetPointLight() == nil, "the actor has no light any more")
		oCheck:Expect(oImage:IsAlive(), "the other components stay")

		-- A body is a physical object, and is found by a ray as the same component
		oBody = oLamp:AddPhysicalBox()
		oBody:SetStatic(true)
		oCheck:Expect(oLamp:GetPhysicalObject() == oBody, "a box is the actor's physical object")
		oCheck:Expect(type(oBody:GetMass()) == "number", "a box answers its base usertype's methods")
		oTransform:SetLocalPosition(Vector3.new(0, -5, 0))

		-- An actor destroyed keeps its components until it is freed, at the end of the frame
		oDoomed = oScene:CreateActor("Doomed", "")
		oDoomedTransform = oDoomed:GetTransform()
		oDoomed:Destroy()
		oCheck:Expect(oDoomedTransform:IsAlive(), "a destroyed actor's components work until the end of the frame")
	elseif nFrame == 4 then
		oCheck:Expect(not oDoomedTransform:IsAlive(), "a freed actor's components are gone")
		oCheck:Expect(IsGone(function() return oDoomedTransform:GetLocalPosition() end), "and raise when used")
		oCheck:Expect(oDoomedTransform == oDoomedTransform, "a handle still equals itself once gone")

		local tHits = Physics.CastRay(Vector3.new(0, 0, 0), Vector3.new(0, -1, 0), 20)
		oCheck:Expect(#tHits == 1 and tHits[1].Object == oBody and tHits[1].Object:IsAlive(), "a ray's hit is a handle on the body")

		-- A value that is no component is refused as before, without being taken for one
		local sError = ErrorOf(function() return oBody.GetMass(Vector3.new(1, 2, 3)) end)
		oCheck:Expect(sError ~= nil and sError:find(kGone, 1, true) == nil, "a value that is no component is refused: " .. tostring(sError))

		local nMajor, nMinor = Application.GetApiVersion()
		oCheck:Expect(nMajor == 1 and nMinor >= 10, "the Lua API is 1.10 or a later 1.x: " .. nMajor .. "." .. nMinor)
		oCheck:Finish()
	end
end

return ComponentRefsCheck
