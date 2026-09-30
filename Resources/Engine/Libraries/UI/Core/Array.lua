--- Array helpers. The table library isn't opened for scripts, so these replace table.insert,
--- table.remove and table.sort for the few operations the UI library needs.
---@class UIArray
local Array = {}

---@param array any[]
---@param value any
function Array.Append(array, value)
	array[#array + 1] = value
end

---@param array any[]
---@param index integer
---@param value any
function Array.InsertAt(array, index, value)
	for i = #array, index, -1 do
		array[i + 1] = array[i]
	end

	array[index] = value
end

---@param array any[]
---@param index integer
function Array.RemoveAt(array, index)
	local count = #array

	for i = index, count - 1 do
		array[i] = array[i + 1]
	end

	array[count] = nil
end

---@param array any[]
---@param value any
---@return integer|nil
function Array.IndexOf(array, value)
	for i = 1, #array do
		if array[i] == value then
			return i
		end
	end

	return nil
end

---@param array any[]
---@param value any
---@return boolean
function Array.RemoveValue(array, value)
	local index = Array.IndexOf(array, value)

	if not index then
		return false
	end

	Array.RemoveAt(array, index)
	return true
end

---@param array any[]
---@return any[]
function Array.Copy(array)
	local copy = {}

	for i = 1, #array do
		copy[i] = array[i]
	end

	return copy
end

--- Stable insertion sort on the key returned by getKey, suited to the short sibling lists of a UI
---@param array any[]
---@param getKey fun(value: any): number
function Array.StableSortBy(array, getKey)
	for i = 2, #array do
		local value = array[i]
		local key = getKey(value)
		local j = i - 1

		while j >= 1 and getKey(array[j]) > key do
			array[j + 1] = array[j]
			j = j - 1
		end

		array[j + 1] = value
	end
end

return Array
