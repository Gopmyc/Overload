/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <doctest/doctest.h>
#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPointLight.h>
#include <OvCore/ECS/Components/UI/CHorizontalLayout.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/Scripting/Lua/LuaApi.h>

namespace
{
	/**
	* A Lua state with the engine's bindings, and a scene to take components from
	*/
	struct ComponentFixture
	{
		sol::state lua;
		OvCore::SceneSystem::Scene scene{ "", "" };

		ComponentFixture()
		{
			OvCore::Scripting::LuaApi::OpenLibraries(lua);
			OvCore::Scripting::LuaApi::Bind(lua);
		}

		/**
		* Runs a chunk and returns what it returns, or the error it raised, as a string
		*/
		std::string Run(const std::string& p_chunk)
		{
			const auto result = lua.safe_script(p_chunk, sol::script_pass_on_error);
			if (!result.valid())
			{
				const sol::error error = result;
				return std::string("error: ") + error.what();
			}

			const sol::object value = result;
			return value.is<std::string>() ? value.as<std::string>() : value.is<bool>() ? (value.as<bool>() ? "true" : "false") : "?";
		}
	};

	constexpr const char* kGoneMessage = "attempt to use a component that was removed, or whose actor was destroyed";
}

TEST_SUITE("Component references")
{
	TEST_CASE_FIXTURE(ComponentFixture, "a component reaches Lua as a handle that works while the component is there")
	{
		auto& actor = scene.CreateActor("Lamp");
		lua["transform"] = &actor.transform;
		auto& light = actor.AddComponent<OvCore::ECS::Components::CPointLight>();
		lua["light"] = &light;

		// A member function, a lambda, and a method of a base usertype
		CHECK(Run("transform:SetLocalPosition(Vector3.new(1, 2, 3)) return tostring(transform:GetLocalPosition().y)") == "2.0");
		CHECK(actor.transform.GetLocalPosition().z == doctest::Approx(3.0f));
		CHECK(Run("light:SetIntensity(4) return tostring(light:GetIntensity())") == "4.0");
		CHECK(light.GetIntensity() == doctest::Approx(4.0f));
		CHECK(Run("return transform:IsAlive() and light:IsAlive()") == "true");
	}

	TEST_CASE_FIXTURE(ComponentFixture, "a removed component raises a clear error instead of being read, and says it is gone")
	{
		auto& actor = scene.CreateActor("Lamp");
		lua["light"] = &actor.AddComponent<OvCore::ECS::Components::CPointLight>();

		REQUIRE(actor.RemoveComponent<OvCore::ECS::Components::CPointLight>());

		CHECK(Run("return light:IsAlive()") == "false");

		// As self of a member function, of a lambda, and of a base usertype's method
		for (const std::string call : { "light:SetIntensity(2)", "light:GetOwner()", "light:GetConstant()" })
		{
			CAPTURE(call);
			const std::string result = Run(call);
			CHECK(result.starts_with("error: "));
			CHECK(result.find(kGoneMessage) != std::string::npos);
		}
	}

	TEST_CASE_FIXTURE(ComponentFixture, "the components of a freed actor are gone with it, the UI ones as well")
	{
		auto& actor = scene.CreateActor("Panel");
		lua["transform"] = &actor.transform;
		lua["layout"] = &actor.AddComponent<OvCore::ECS::Components::UI::CHorizontalLayout>();

		CHECK(Run("layout:SetSpacing(6) return layout:IsAlive()") == "true");
		REQUIRE(scene.DestroyActor(actor));

		CHECK(Run("return transform:IsAlive() or layout:IsAlive()") == "false");
		CHECK(Run("return transform:GetLocalPosition()").find(kGoneMessage) != std::string::npos);
		CHECK(Run("layout:SetSpacing(4)").find(kGoneMessage) != std::string::npos);
	}

	TEST_CASE_FIXTURE(ComponentFixture, "two handles on one component are equal, whatever type they were pushed as")
	{
		auto& actor = scene.CreateActor("Lamp");
		auto& light = actor.AddComponent<OvCore::ECS::Components::CPointLight>();
		auto& other = scene.CreateActor("Other").AddComponent<OvCore::ECS::Components::CPointLight>();

		lua["first"] = &light;
		lua["again"] = &light;
		lua["asLight"] = static_cast<OvCore::ECS::Components::CLight*>(&light);
		lua["other"] = &other;

		CHECK(Run("return first == again and first == asLight and first ~= other") == "true");

		// Still equal once gone, and still not equal to what replaces it
		actor.RemoveComponent<OvCore::ECS::Components::CPointLight>();
		lua["replacement"] = &actor.AddComponent<OvCore::ECS::Components::CPointLight>();
		CHECK(Run("return first == again and first ~= replacement") == "true");
	}

	TEST_CASE_FIXTURE(ComponentFixture, "a value that is not the component a method expects is refused, and nil stays nil")
	{
		auto& actor = scene.CreateActor("Lamp");
		lua["light"] = &actor.AddComponent<OvCore::ECS::Components::CPointLight>();
		lua["nothing"] = static_cast<OvCore::ECS::Components::CPointLight*>(nullptr);
		lua["transform"] = &actor.transform;

		CHECK(Run("return tostring(nothing)") == "nil");

		// As self: another userdata, nil, and a component of another type
		for (const std::string call : { "light.SetIntensity(Vector3.new(1, 2, 3), 2)", "light.SetIntensity(nil, 2)", "light.SetIntensity(transform, 2)" })
		{
			CAPTURE(call);
			const std::string result = Run(call);
			CHECK(result.starts_with("error: "));
			CHECK(result.find(kGoneMessage) == std::string::npos);
		}
	}
}
