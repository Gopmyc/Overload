-- A trigger across the Character check's path, which writes down who comes into it
local Log = require("Scripts/Checks/CharacterLog")

local CharacterGate = {}

function CharacterGate:OnAwake()
	local oBody = self.owner:AddPhysicalBox()
	oBody:SetSize(Vector3.new(0.2, 1, 2))
	oBody:SetStatic(true)
	oBody:SetTrigger(true)
end

function CharacterGate:OnTriggerEnter(oOther)
	table.insert(Log.tEntered, oOther:GetOwner():GetName())
end

return CharacterGate
