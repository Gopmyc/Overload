---@meta

--- Declares a list property of a Behaviour: the inspector edits its items as one comma-separated line (a string item
--- holds no comma), and the script receives a plain table. Raises an error for another item type, or an item not of it
---@param itemType "number"|"string"|"bool"
---@param items? (number|string|boolean)[] empty when nil
---@return table
function List(itemType, items) end
