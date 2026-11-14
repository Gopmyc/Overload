--- Files of the save folder (the engine's Storage) that keep the version of their shape, as JSON (the engine's Json),
--- and move from one version to the next by the game's migrations.
---
--- ```lua
--- local Std = require(":Libraries/Std/Init")
---
--- local settings = Std.SaveFile.New("settings.json", {
---     version = 2,
---     migrations = {
---         [0] = function(data) data.Version = nil; return data end, -- a file from before the envelope
---         [1] = function(data) data.volume = data.volume or 1; return data end,
---     },
--- })
---
--- local data = settings:Read() or Defaults()          -- migrated to version 2, or nil
--- settings:Schedule(function() return Build() end)   -- written a second after the last ask
--- ```
---
--- - A file holds `{"version": n, "data": ...}`. Any other table counts as version 0, whole: a file written before the
---   envelope reaches the game through the migration from 0.
--- - `migrations[n]` takes the data of version n and returns that of version n + 1 (it may change the table it gets).
---   A file of an older version is migrated when it is read, and written in the current one the next time it is.
--- - A file the game cannot use is set aside (copied beside it, once a run under each name), with a warning, and Read
---   returns nil and why: the game starts from its defaults, and its next write replaces the file. Unreadable, or not
---   a table: `<name>.broken.json`. Of a newer version than the game's (the game went back a version):
---   `<name>.v<n>.json`. A migration missing, failing or returning no table: `<name>.broken.json`.
--- - A write asked for again and again (a slider dragged) is made once, a moment of real time after the last ask
---   (Schedule), or at once by Flush. Whatever still waits is written when the game ends (Events.AddQuit).
---@class StdSaveFileModule
local SaveFile = {}

local Timers = require(":Libraries/Std/Timers")

-- Seconds of real time between the last Schedule and the write
local kDefaultDelay = 1.0

local defined = {}
local order = {}

---@class StdSaveFile
local File = {}
File.__index = File

local function Fail(message, level)
	error("Std.SaveFile: " .. message, level + 1)
end

--- The name a file is set aside under: "settings.json" and "broken" give "settings.broken.json"
local function AsideName(name, suffix)
	local stem, extension = name:match("^(.*)(%.[^./]+)$")

	if stem ~= nil and stem ~= "" and stem:sub(-1) ~= "/" then
		return stem .. "." .. suffix .. extension
	end

	return name .. "." .. suffix
end

--- The version and the data of a decoded file: an envelope's, or 0 and the whole table
local function Unwrap(decoded)
	local version, data = decoded.version, decoded.data

	if math.type(version) == "integer" and version >= 1 and type(data) == "table" then
		local count = 0

		for _ in pairs(decoded) do
			count = count + 1
		end

		if count == 2 then
			return version, data
		end
	end

	return 0, decoded
end

--- Logs a warning about this file, once for each trouble (the same message again is not logged), and returns nil
--- and the reason
function File:Warn(reason, message)
	if not self.m_warned[reason] then
		self.m_warned[reason] = true
		Debug.LogWarning("Std.SaveFile: " .. self.m_name .. ": " .. (message or reason))
	end

	return nil, reason
end

--- Copies a file the game cannot use beside it, once for each name it is set aside under, warns once for each
--- reason, and returns nil and the reason
function File:SetAside(text, suffix, reason)
	local aside = AsideName(self.m_name, suffix)
	local message = reason .. "; kept aside as " .. aside .. " earlier, the defaults stand"

	if not self.m_setAside[aside] then
		self.m_setAside[aside] = true
		local written, writeError = Storage.Write(aside, text)

		if written then
			message = reason .. "; set aside as " .. aside .. ", the defaults stand"
		else
			message = reason .. "; it could not be set aside as " .. aside .. " (" .. tostring(writeError) .. "), the defaults stand"
		end
	end

	return self:Warn(reason, message)
end

--- Returns the file's data, migrated to the current version, or nil (and why) when there is none to use: no file, or
--- one set aside
---@return table|nil data
---@return string|nil reason
function File:Read()
	if not Storage.Exists(self.m_name) then
		return nil, "no file"
	end

	local text, readError = Storage.Read(self.m_name)

	if text == nil then
		return self:Warn("cannot be read: " .. tostring(readError))
	end

	local decoded, decodeError = Json.Decode(text)

	if type(decoded) ~= "table" then
		return self:SetAside(text, "broken", "not a table of JSON: " .. tostring(decodeError or decoded))
	end

	local version, data = Unwrap(decoded)

	if version > self.m_version then
		return self:SetAside(text, "v" .. version, "version " .. version .. " is newer than the game's, " .. self.m_version)
	end

	for from = version, self.m_version - 1 do
		local migrate = self.m_migrations[from]

		if migrate == nil then
			return self:SetAside(text, "broken", "no migration from version " .. from)
		end

		local ok, result = pcall(migrate, data)

		if not ok then
			return self:SetAside(text, "broken", "the migration from version " .. from .. " failed: " .. tostring(result))
		end

		if type(result) ~= "table" then
			return self:SetAside(text, "broken", "the migration from version " .. from .. " returned no table")
		end

		data = result
	end

	return data
end

--- Writes the data at once, in the current version. Returns true, or nil and why (logged once)
---@param data table
---@return boolean|nil written
---@return string|nil reason
function File:Write(data)
	if type(data) ~= "table" then
		Fail("Write takes a table", 2)
	end

	local text, encodeError = Json.Encode({ version = self.m_version, data = data }, true)

	if text == nil then
		return self:Warn("cannot be written: " .. tostring(encodeError))
	end

	local written, writeError = Storage.Write(self.m_name, text)

	if not written then
		return self:Warn("cannot be written: " .. tostring(writeError))
	end

	return true
end

--- Writes what build returns, a moment of real time after the last ask for this file (1 second by default)
---@param build fun(): table
---@param delay number|nil seconds
function File:Schedule(build, delay)
	if type(build) ~= "function" then
		Fail("Schedule takes a function that returns the data", 2)
	end

	if delay ~= nil and (type(delay) ~= "number" or delay < 0) then
		Fail("Schedule takes a delay of 0 seconds or more", 2)
	end

	self.m_pending = build

	if self.m_timer ~= nil then
		self.m_timer:Cancel()
	end

	self.m_timer = Timers.After(delay or kDefaultDelay, function() self:Flush() end, { unscaled = true })
end

--- Returns true while a write waits
---@return boolean
function File:IsPending()
	return self.m_pending ~= nil
end

--- Writes at once what waits, if anything. Returns true, or nil and why
---@return boolean|nil written
---@return string|nil reason
function File:Flush()
	local build = self.m_pending

	if build == nil then
		return true
	end

	self.m_pending = nil

	if self.m_timer ~= nil then
		self.m_timer:Cancel()
		self.m_timer = nil
	end

	local ok, data = pcall(build)

	if not ok then
		Debug.LogError("Std.SaveFile: " .. self.m_name .. ": the function that builds it failed, nothing is written: " .. tostring(data))
		return nil, tostring(data)
	end

	if type(data) ~= "table" then
		Debug.LogError("Std.SaveFile: " .. self.m_name .. ": the function that builds it returned no table, nothing is written")
		return nil, "no table"
	end

	return self:Write(data)
end

---@return string
function File:GetName()
	return self.m_name
end

---@return integer
function File:GetVersion()
	return self.m_version
end

--- Defines a save file: its name in the save folder, and its version (1 or more) with the migrations that reach it
---@param name string
---@param spec {version: integer, migrations: table<integer, fun(data: table): table>|nil}
---@return StdSaveFile
function SaveFile.New(name, spec)
	if type(name) ~= "string" or name == "" then
		Fail("New takes the file's name in the save folder", 2)
	end

	if defined[name] ~= nil then
		Fail("\"" .. name .. "\" is defined already", 2)
	end

	if type(spec) ~= "table" then
		Fail("New takes a table with the version", 2)
	end

	if math.type(spec.version) ~= "integer" or spec.version < 1 then
		Fail("the version is a whole number of 1 or more", 2)
	end

	local migrations = spec.migrations or {}

	if type(migrations) ~= "table" then
		Fail("the migrations are a table", 2)
	end

	for from, migrate in pairs(migrations) do
		if math.type(from) ~= "integer" or from < 0 or from >= spec.version then
			Fail("a migration goes from a version of 0 to " .. (spec.version - 1) .. ": " .. tostring(from), 2)
		end

		if type(migrate) ~= "function" then
			Fail("the migration from version " .. from .. " is a function", 2)
		end
	end

	local file = setmetatable({
		m_name = name,
		m_version = spec.version,
		m_migrations = migrations,
		m_pending = nil,
		m_timer = nil,
		m_warned = {},
		m_setAside = {}
	}, File)

	defined[name] = file
	order[#order + 1] = file
	return file
end

--- Writes at once what waits for every file
function SaveFile.FlushAll()
	for _, file in ipairs(order) do
		file:Flush()
	end
end

-- Whatever waits is written as the game ends
Events.AddQuit(SaveFile.FlushAll)

return SaveFile
