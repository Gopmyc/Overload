---@meta

--- Plays an animation graph on its actor's skinned mesh: a JSON file of the project (.ovanim) whose layers hold
--- states, each a clip or clips blended along a parameter, and transitions between them on the parameters a script
--- sets (see Docs/Animation.md). While it plays, the skinned mesh renderer samples the graph's clips instead of its own
--- layers, with the same bone matching. Moved on once a frame, after every OnUpdate and before every OnLateUpdate, so
--- that a parameter set in OnUpdate applies in the same frame. A layer is given by its index, from 0, or its name; the
--- first layer when none is given
---@class Animator : Component
Animator = {}

--- Returns the actor that owns this component
---@return Actor
function Animator:GetOwner() end

--- Sets the graph file (an asset path), read again on next use
---@param path string
function Animator:SetGraphPath(path) end

--- Returns the graph file
---@return string
function Animator:GetGraphPath() end

--- Sets the model whose clips the graph plays, or nil for the rendered model
---@overload fun(self: Animator, model: Model|nil)
---@overload fun(self: Animator, asset: AssetRef)
function Animator:SetAnimationSourceModel(...) end

--- Returns the model whose clips the graph plays, or nil for the rendered model
---@return Model|nil
function Animator:GetAnimationSourceModel() end

--- Sets which clip bone drives each model bone, for clips of another rig: { ModelBone = "ClipBone" }
---@param map table<string, string>
function Animator:SetBoneMap(map) end

--- Returns which clip bone drives each model bone
---@return table<string, string>
function Animator:GetBoneMap() end

--- Reads the graph file again, from its default states
function Animator:Reload() end

--- Returns true when the graph is loaded and plays
---@return boolean
function Animator:IsLoaded() end

--- Returns why the graph does not play (no file, a mistake in it with its JSON path, an unknown clip, no skinned mesh),
--- or nil
---@return string|nil
function Animator:GetError() end

--- Sets a number parameter, returning false for an unknown parameter or one of another kind
---@param name string
---@param value number
---@return boolean
function Animator:SetFloat(name, value) end

--- Returns a number parameter, or nil
---@param name string
---@return number|nil
function Animator:GetFloat(name) end

--- Sets a boolean parameter, returning false for an unknown parameter or one of another kind
---@param name string
---@param value boolean
---@return boolean
function Animator:SetBool(name, value) end

--- Returns a boolean parameter, or nil
---@param name string
---@return boolean|nil
function Animator:GetBool(name) end

--- Sets a trigger, until a transition uses it up or ResetTrigger. Returns false for an unknown trigger
---@param name string
---@return boolean
function Animator:SetTrigger(name) end

--- Clears a trigger. Returns false for an unknown trigger
---@param name string
---@return boolean
function Animator:ResetTrigger(name) end

--- Returns the names of the parameters
---@return string[]
function Animator:GetParameterNames() end

--- Returns the names of the layers
---@return string[]
function Animator:GetLayerNames() end

--- Returns the state a layer is in (the one it goes to while it fades), or nil
---@param layer? integer|string
---@return string|nil
function Animator:GetState(layer) end

--- Returns how far a layer's state has played, in its normalized time (1 per loop)
---@param layer? integer|string
---@return number
function Animator:GetStateTime(layer) end

--- Returns true while a layer fades from a state to another
---@param layer? integer|string
---@return boolean
function Animator:IsInTransition(layer) end

--- Sets how much a layer covers the layers before it (0 to 1), returning false for an unknown layer
---@param layer integer|string
---@param weight number
---@return boolean
function Animator:SetLayerWeight(layer, weight) end

--- Returns how much a layer covers the layers before it, or nil
---@param layer? integer|string
---@return number|nil
function Animator:GetLayerWeight(layer) end

--- Moves to a state at once, over a fade (seconds, 0 by default), whatever the transitions say: in the given layer,
--- or in the first layer that has it. Returns false for an unknown state or layer
---@param state string
---@param fade? number
---@param layer? integer|string
---@return boolean
function Animator:Play(state, fade, layer) end

--- Puts every parameter back to its default and every layer in its default state
function Animator:Reset() end
