-- A destroyed actor, and its children, leave the scene at the end of the frame
local Check = require("Scripts/Checks/Check")

local DestroyCheck = {}
local oCheck = Check.New("Destroy")
local oDoomed, oChild, nFrame = nil, nil, 0

function DestroyCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1
	local oScene = Scenes.GetCurrentScene()

	if nFrame == 2 then
		oDoomed = oScene:CreateActor("Doomed", "")
		oChild = oScene:CreateActor("DoomedChild", "")
		oChild:SetParent(oDoomed)
		oCheck:Equal(#oScene:FindActorsByName("Doomed"), 1, "actors made")
	elseif nFrame == 3 then
		oDoomed:Destroy()
		oCheck:Expect(not oDoomed:IsAlive(), "a destroyed actor is no longer alive")
		oCheck:Expect(not oChild:IsAlive(), "the child of a destroyed actor is destroyed too")
	elseif nFrame == 5 then
		oCheck:Equal(#oScene:FindActorsByName("Doomed"), 0, "destroyed actors left in the scene")
		oCheck:Equal(#oScene:FindActorsByName("DoomedChild"), 0, "destroyed children left in the scene")
		oCheck:Expect(oScene:FindActorByName("Check") ~= nil, "the other actors stay")
		oCheck:Finish()
	end
end

return DestroyCheck
