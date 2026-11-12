-- The engine's shaders compile, every variant of them (a shader compiles all its variants when it loads, and logs an
-- error for one that fails); a shader built on Standard's extension points compiles and draws; the fog is a material
-- option of Standard and Unlit; an additive and a premultiplied material draw in the transparent pass
local Check = require("Scripts/Checks/Check")

local ShadersCheck = {}
local oCheck = Check.New("Shaders")
local nFrame = 0

local tEngineShaders = {
	"Atmosphere", "Image", "Particle", "ShadowFallback", "Skysphere", "Standard", "Text", "Unlit",
	"PostProcess/ApplyExposure", "PostProcess/AutoExposure", "PostProcess/Blit", "PostProcess/Bloom",
	"PostProcess/BloomDownsampling", "PostProcess/BloomUpsampling", "PostProcess/FXAA", "PostProcess/Luminance",
	"PostProcess/Tonemapping",
}

local function Object(sName, sModel, sMaterial, vPosition, vScale)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(vPosition)
	oActor:GetTransform():SetLocalScale(vScale or Vector3.new(1, 1, 1))
	oActor:AddModelRenderer():SetModel(Resources.GetModel(sModel))
	local oMaterial = Resources.GetMaterial(sMaterial)
	oCheck:Expect(oMaterial ~= nil, sMaterial .. " loads")
	oActor:AddMaterialRenderer():SetMaterial(0, oMaterial)
end

function ShadersCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()

	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:GetTransform():SetLocalPosition(Vector3.new(0, 3, -9))
	oCamera:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(15, 0, 0)))
	oCamera:AddCamera()

	local oSun = oScene:CreateActor("Sun", "")
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(50, -30, 0)))
	oSun:AddDirectionalLight()

	local oStack = oScene:CreateActor("PostProcess", ""):AddPostProcessStack()
	local oFog = oStack:GetFogSettings()
	oFog.Enabled = true
	oFog.Color = Vector3.new(0.7, 0.75, 0.8)
	oFog.Start = 6
	oFog.Density = 0.08
	oFog.MaxOpacity = 0.9
	oFog.Intensity = 1
	oStack:SetFogSettings(oFog)

	Object("Ground", ":Models/Cube.fbx", "Materials/Ground.ovmat", Vector3.new(0, -0.5, 6), Vector3.new(30, 0.2, 30))
	Object("Hooked", ":Models/Sphere.fbx", "Materials/Hooked.ovmat", Vector3.new(0, 0.8, 1))
	Object("Fogged", ":Models/Sphere.fbx", "Materials/Standard.ovmat", Vector3.new(-3, 1, 14))
	Object("NotFogged", ":Models/Sphere.fbx", "Materials/StandardNoFog.ovmat", Vector3.new(3, 1, 14))
	Object("UnlitFogged", ":Models/Cube.fbx", "Materials/UnlitFog.ovmat", Vector3.new(0, 0.5, 18))
	Object("Additive", ":Models/Cube.fbx", "Materials/Additive.ovmat", Vector3.new(-2.2, 1.6, 2), Vector3.new(1.6, 1.6, 0.1))
	Object("Premultiplied", ":Models/Cube.fbx", "Materials/Premultiplied.ovmat", Vector3.new(2.2, 1.6, 2), Vector3.new(1.6, 1.6, 0.1))
end

function ShadersCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame == 2 then
		for _, sName in ipairs(tEngineShaders) do
			oCheck:Expect(Resources.GetShader(":Shaders/" .. sName .. ".ovfx") ~= nil, sName .. " loads")
		end
		oCheck:Expect(Resources.GetShader("Shaders/Hooked.ovfx") ~= nil, "a shader built on Standard's extension points loads")
	elseif nFrame == 10 then
		-- Ten frames drawn: every pass (shadows, depth pre-pass, opaque, transparent) has used the materials above
		oCheck:Finish()
	end
end

return ShadersCheck
