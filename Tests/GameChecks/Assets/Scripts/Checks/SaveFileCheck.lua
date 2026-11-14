-- The standard library's SaveFile in a game, on the checks' save folder: a file written and read back in its envelope,
-- one from before the envelope migrated, a newer one and a broken one set aside, the next write replacing them, a
-- write asked for twice made once, and one left waiting, which the game's end writes. The check quits, and gives its
-- verdict from its own quit callback, which runs after the library's
local Check = require("Scripts/Checks/Check")
local Std = require(":Libraries/Std/Init")

local SaveFileCheck = {}
local oCheck = Check.New("SaveFile")
local nFrame = 0
local oLast = nil

-- The files the check writes, removed first: the editor keeps its save folder from one game to the next
local tFiles = { "check_settings.json", "check_settings.v4.json", "check_settings.broken.json", "check_last.json" }

function SaveFileCheck:OnStart()
	for _, sFile in ipairs(tFiles) do
		if Storage.Exists(sFile) then
			Storage.Delete(sFile)
		end
	end

	local oSettings = Std.SaveFile.New("check_settings.json", { version = 2, migrations = {
		[0] = function(tData) tData.Version = nil; tData.from = 0; return tData end,
		[1] = function(tData) tData.volume = tData.volume or 1; return tData end,
	} })

	-- Written in its envelope, read back
	oCheck:Expect(oSettings:Read() == nil, "no file at first")
	oCheck:Expect(oSettings:Write({ volume = 0.5 }), "written")
	local tFile = Json.Decode(Storage.Read("check_settings.json"))
	oCheck:Equal(tFile.version, 2, "written at its version")
	oCheck:Equal(tFile.data.volume, 0.5, "its data in the envelope")
	oCheck:Equal(oSettings:Read().volume, 0.5, "read back")

	-- From before the envelope, as the game's files are
	Storage.Write("check_settings.json", Json.Encode({ Version = 1, speed = 3 }))
	local tLegacy = oSettings:Read()
	oCheck:Expect(tLegacy.from == 0 and tLegacy.Version == nil and tLegacy.volume == 1 and tLegacy.speed == 3, "migrated from version 0")

	-- Newer, then broken: set aside, then replaced by the next write
	Storage.Write("check_settings.json", Json.Encode({ version = 4, data = {} }))
	local tNewer, sWhy = oSettings:Read()
	oCheck:Expect(tNewer == nil and tostring(sWhy):find("newer") ~= nil, "a newer file is not used: " .. tostring(sWhy))
	oCheck:Expect(Storage.Exists("check_settings.v4.json"), "and is set aside")
	Storage.Write("check_settings.json", "{ nope")
	oCheck:Expect(oSettings:Read() == nil and Storage.Exists("check_settings.broken.json"), "a broken file is set aside")
	oCheck:Expect(oSettings:Write({ volume = 0.75 }) and oSettings:Read().volume == 0.75, "the next write replaces it")

	-- Asked for twice, made once
	local nBuilds = 0
	oSettings:Schedule(function() nBuilds = nBuilds + 1; return { volume = 0.1 } end)
	oSettings:Schedule(function() nBuilds = nBuilds + 1; return { volume = 0.2 } end)
	oCheck:Expect(oSettings:IsPending(), "a write waits")
	oSettings:Flush()
	oCheck:Equal(nBuilds, 1, "built once")
	oCheck:Equal(oSettings:Read().volume, 0.2, "the last ask written")

	-- One left waiting for the game's end
	oLast = Std.SaveFile.New("check_last.json", { version = 1 })
	oLast:Schedule(function() return { written = "at the end" } end, 600)

	-- The library's callback came first, when it was loaded
	Events.AddQuit(function()
		oCheck:Expect(not oLast:IsPending(), "nothing waits once the game ends")
		local tLast = oLast:Read()
		oCheck:Equal(tLast and tLast.written, "at the end", "what waited is written as the game ends")
		oCheck:Finish()
	end)
end

function SaveFileCheck:OnUpdate()
	nFrame = nFrame + 1

	if nFrame == 3 then
		oCheck:Expect(oLast:IsPending(), "still waiting a few frames later")
		oCheck:Expect(not Storage.Exists("check_last.json"), "and not written yet")

		-- The verdict comes from the game's end
		Application.Quit()
	end
end

return SaveFileCheck
