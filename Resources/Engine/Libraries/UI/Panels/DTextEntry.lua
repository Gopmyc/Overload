local Array = Resources.GetScript(":Libraries/UI/Core/Array.lua")
local Keyboard = Resources.GetScript(":Libraries/UI/Core/Keyboard.lua")
local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local Skin = Resources.GetScript(":Libraries/UI/Core/Skin.lua")

Resources.GetScript(":Libraries/UI/Panels/DLabel.lua")

--- Editable single line text field. The caret is rendered as a character inserted in the text, since
--- text can't be measured from Lua (see Docs/LuaExposureGaps.md).
---@class DTextEntry : DLabel
local DTextEntry = {}

local kCaret = "|"

-- The value is stored as a list of characters. Without the string library, a text given to SetText
-- can't be split, so it is kept as a single entry, edited as a whole.
local function RebuildValue(self)
	local text = ""

	for i = 1, #self.m_Characters do
		text = text .. self.m_Characters[i]
	end

	self.m_Text = text
end

local function OnValueEdited(self)
	RebuildValue(self)
	self:RefreshText()
	self:InvalidatePaint()
	self:OnChange()
end

local function MoveCaret(self, position)
	position = math.max(0, math.min(position, #self.m_Characters))

	if self.m_Caret ~= position then
		self.m_Caret = position
		self:RefreshText()
	end
end

function DTextEntry:Init()
	self:SetPaintBackground(true)
	self:SetMouseInputEnabled(true)
	self:SetKeyboardInputEnabled(true)
	self:SetCursor(CursorShape.IBEAM)

	self.m_Characters = {}
	self.m_Caret = 0
	self.m_PlaceholderText = ""

	self:RefreshText()
end

--- Replaces the value without calling OnChange
---@param text string
function DTextEntry:SetText(text)
	text = tostring(text)

	if self.m_Text == text then
		return
	end

	self.m_Characters = text == "" and {} or { text }
	self.m_Caret = #self.m_Characters
	self.m_Text = text
	self:RefreshText()
	self:InvalidatePaint()
end

--- Text displayed while the value is empty and the entry doesn't have the focus
---@param text string
function DTextEntry:SetPlaceholderText(text)
	self.m_PlaceholderText = tostring(text)
	self:RefreshText()
	self:InvalidatePaint()
end

---@return string
function DTextEntry:GetPlaceholderText()
	return self.m_PlaceholderText
end

---@return boolean
function DTextEntry:IsShowingPlaceholder()
	return self.m_Text == "" and not self:HasFocus()
end

---@return string
function DTextEntry:GetDisplayedText()
	if self:IsShowingPlaceholder() then
		return self.m_PlaceholderText
	end

	if not self:HasFocus() then
		return self.m_Text
	end

	local text = ""

	for i = 1, self.m_Caret do
		text = text .. self.m_Characters[i]
	end

	text = text .. kCaret

	for i = self.m_Caret + 1, #self.m_Characters do
		text = text .. self.m_Characters[i]
	end

	return text
end

---@return Vector4
function DTextEntry:GetSkinTextColor()
	if self.m_Enabled and self:IsShowingPlaceholder() then
		return Skin.TextPlaceholder
	end

	return DTextEntry.BaseClass.GetSkinTextColor(self)
end

---@return Vector4
function DTextEntry:GetSkinBackgroundColor()
	return self:HasFocus() and Skin.TextEntryBackgroundFocused or Skin.TextEntryBackground
end

---@param gained boolean
function DTextEntry:OnFocusChanged(gained)
	self:RefreshText()
end

--- The caret can't be placed under the cursor, clicking moves it to the end of the value
---@param button MouseButton
function DTextEntry:OnMousePressed(button)
	MoveCaret(self, #self.m_Characters)
end

---@param key Key
function DTextEntry:OnKeyCodePressed(key)
	if key == Key.BACKSPACE then
		if self.m_Caret > 0 then
			Array.RemoveAt(self.m_Characters, self.m_Caret)
			self.m_Caret = self.m_Caret - 1
			OnValueEdited(self)
		end
	elseif key == Key.DELETE then
		if self.m_Caret < #self.m_Characters then
			Array.RemoveAt(self.m_Characters, self.m_Caret + 1)
			OnValueEdited(self)
		end
	elseif key == Key.LEFT then
		MoveCaret(self, self.m_Caret - 1)
	elseif key == Key.RIGHT then
		MoveCaret(self, self.m_Caret + 1)
	elseif key == Key.HOME then
		MoveCaret(self, 0)
	elseif key == Key.END then
		MoveCaret(self, #self.m_Characters)
	elseif key == Key.ENTER or key == Key.KP_ENTER then
		self:OnEnter(self.m_Text)
	else
		local character = Keyboard.ToCharacter(key)

		if character and not self:AllowInput(character) then
			self.m_Caret = self.m_Caret + 1
			Array.InsertAt(self.m_Characters, self.m_Caret, character)
			OnValueEdited(self)
		end
	end
end

--- Returns true to prevent the character from being typed
---@param character string
---@return boolean|nil
function DTextEntry:AllowInput(character) end

--- Called after the value has been edited by the user
function DTextEntry:OnChange() end

--- Called when Enter is pressed while the entry has the focus
---@param value string
function DTextEntry:OnEnter(value) end

return Registry.Register("DTextEntry", DTextEntry, "DLabel")
