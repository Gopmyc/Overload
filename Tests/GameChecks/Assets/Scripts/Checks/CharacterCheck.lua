-- A character controller from a scene file, driven from Lua: it lands on the floor and stands, walks through a body of
-- a layer it ignores and through a trigger, climbs a step, is stopped by a wall, jumps and lands, takes an impulse
local Check = require("Scripts/Checks/Check")
local Log = require("Scripts/Checks/CharacterLog")

local CharacterCheck = {}
local oCheck = Check.New("Character")
local nStep = 0
local nJumpStep = nil
local nHighest = 0

-- The character stands half its straight part plus its radius above the ground
local STANDING = 0.5 + 0.4
local STEP_TOP = 0.2

local function Box(sName, vPosition, vHalfExtents, sLayer)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vPosition)
	local oBody = oActor:AddPhysicalBox()
	oBody:SetSize(vHalfExtents)
	oBody:SetStatic(true)
	oBody:SetCollisionLayer(Physics.GetLayerIndex(sLayer))
end

function CharacterCheck:OnAwake()
	Box("Floor", Vector3.new(0, -0.5, 0), Vector3.new(10, 0.5, 10), "Floor")
	Box("Ghost", Vector3.new(0.8, 0.3, 0), Vector3.new(0.2, 0.3, 2), "Enemy")
	Box("Step", Vector3.new(4, STEP_TOP * 0.5, 0), Vector3.new(2, STEP_TOP * 0.5, 3), "Default")
	Box("Wall", Vector3.new(5.5, 2, 0), Vector3.new(0.5, 2, 3), "Default")
end

function CharacterCheck:OnFixedUpdate(nDelta)
	nStep = nStep + 1

	local oHero = Scenes.GetCurrentScene():FindActorByName("Hero")
	local oController = oHero:GetCharacterController()
	local vPosition = oHero:GetTransform():GetWorldPosition()

	if nStep == 1 then
		-- The settings of the file
		oCheck:Expect(oController ~= nil, "the hero has a character controller")
		oCheck:Expect(oHero:GetPhysicalObject() ~= nil, "it is the hero's physical object")
		oCheck:Near(oController:GetRadius(), 0.4, "radius")
		oCheck:Near(oController:GetHeight(), 1, "height")
		oCheck:Near(oController:GetMaxSlopeAngle(), 45, "max slope angle")
		oCheck:Near(oController:GetStepHeight(), 0.25, "step height")
		oCheck:Near(oController:GetGroundSnapDistance(), 0.2, "ground snap distance")
		oCheck:Near(oController:GetGravityFactor(), 1, "gravity factor, absent from the file")
		oCheck:Near(oController:GetPushStrength(), 150, "push strength")
		oCheck:Near(oController:GetMass(), 80, "mass")
		oCheck:Equal(oController:GetCollisionLayer(), Physics.GetLayerIndex("Player"), "layer")
	elseif nStep == 30 then
		-- Landed and standing
		oCheck:Expect(oController:IsGrounded(), "grounded once landed")
		oCheck:Equal(oController:GetGroundState(), CharacterGroundState.ON_GROUND, "ground state")
		local oGround = oController:GetGroundObject()
		oCheck:Equal(oGround and oGround:GetOwner():GetName(), "Floor", "ground object")
		oCheck:Near(oController:GetGroundNormal().y, 1, "ground normal")
		oCheck:Near(vPosition.y, STANDING, "standing height", 0.01)
		oCheck:Near(oController:GetLinearVelocity().y, 0, "no vertical velocity on the ground")

		local tHits = Physics.CastRay(Vector3.new(-3, STANDING, 0), Vector3.new(1, 0, 0), 10)
		oCheck:Equal(tHits[1] and tHits[1].Object:GetOwner():GetName(), "Hero", "a ray meets the character")

		oController:SetLinearVelocity(Vector3.new(4, 0, 0))
	elseif nStep == 30 + 90 then
		-- Through the ghost and the gate, up the step, against the wall
		-- It keeps 2 cm from a wall
		oCheck:Near(vPosition.x, 5 - 0.4 - 0.02, "stopped by the wall", 0.01)
		oCheck:Near(vPosition.y, STEP_TOP + STANDING, "on the step", 0.02)
		oCheck:Near(oController:GetLinearVelocity().x, 4, "the velocity given is kept")
		oCheck:Near(oController:GetRealVelocity().x, 0, "the real velocity says it does not move", 0.05)
		oCheck:Equal(table.concat(Log.tEntered, ","), "Hero", "the gate saw the hero come in once")

		oController:SetLinearVelocity(Vector3.new(0, 4, 0))
		nJumpStep = nStep
		nHighest = vPosition.y
	elseif nJumpStep ~= nil and nStep == nJumpStep + 1 then
		oCheck:Expect(not oController:IsGrounded(), "the jump leaves the ground")
	elseif nJumpStep ~= nil and nStep < nJumpStep + 60 then
		nHighest = math.max(nHighest, vPosition.y)
	elseif nJumpStep ~= nil and nStep == nJumpStep + 60 then
		-- v^2 / 2g above the step
		oCheck:Near(nHighest, STEP_TOP + STANDING + 16 / (2 * 9.81), "jump height", 0.05)
		oCheck:Expect(oController:IsGrounded(), "landed")

		oController:AddImpulse(Vector3.new(0, 0, 160))
		oCheck:Near(oController:GetLinearVelocity().z, 2, "an impulse over the mass")
		oCheck:Finish()
	end
end

return CharacterCheck
