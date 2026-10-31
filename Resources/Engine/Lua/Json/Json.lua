---@meta

--- JSON (RFC 8259) to and from Lua values, to save tables through Storage. Nothing but data comes out of a decode:
--- no code is ever run.
---
--- Encoding: nil, booleans, numbers, strings and tables. A table whose keys are exactly 1 to n is a list, one whose keys
--- are all strings an object, written in key order; an empty table is []. A table mixing both, with holes, with other
--- keys, or nested deeper than 64 (a cycle) is refused, as are functions, userdata, infinite numbers and strings that
--- are not UTF-8. Integers stay integers, and a float is written so that it reads back as the same float.
---
--- Decoding: strict JSON, 4 MiB at most. An object's keys stay strings ({"1": 2} gives { ["1"] = 2 }); an integer
--- without fraction or exponent gives an integer; a member set to null is left out; a null in a list, a repeated key,
--- a byte order mark or anything after the value is refused.
---
--- Neither raises an error: they return nil and what went wrong, "json: ..." with where it happened.
---@class Json
Json = {}

--- The value as JSON text, indented with tabs when pretty
---@param value any
---@param pretty? boolean
---@return string|nil text
---@return string|nil error
function Json.Encode(value, pretty) end

--- The value the JSON text holds
---@param text string
---@return any value
---@return string|nil error
function Json.Decode(text) end
