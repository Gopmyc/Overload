/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/UI/CCanvas.h>
#include <OvCore/ECS/Components/UI/CText.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/UIRenderingUtils.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/ResourceManagement/UIResourceRegistry.h>
#include <OvCore/SceneSystem/Scene.h>

using OvCore::ECS::Components::UI::CCanvas;
using OvCore::ECS::Components::UI::CText;
using OvCore::Rendering::UIRenderingUtils::IsDrawnLit;
using OvCore::Rendering::UIRenderingUtils::ResolvedUIElement;

namespace
{
	/**
	* The services a text reads, with no lit material from the host unless one is named. Static: what other suites
	* read from the locator afterwards stays alive
	*/
	void ProvideUIServices(const std::string& p_litTextMaterialPath = "")
	{
		static OvCore::ResourceManagement::UIResourceRegistry registry;
		static OvCore::ResourceManagement::MaterialManager materialManager;

		registry.ProvideDefinition({ .litTextMaterialPath = p_litTextMaterialPath });
		OvCore::Global::ServiceLocator::Provide(registry);
		OvCore::Global::ServiceLocator::Provide(materialManager);
	}

	/**
	* A text saved, and read back into another
	*/
	bool SaveAndReadLit(CText& p_text, CText& p_other)
	{
		tinyxml2::XMLDocument document;
		auto* node = document.NewElement("data");
		document.InsertFirstChild(node);
		p_text.OnSerialize(document, node);
		p_other.OnDeserialize(document, node);
		return node->FirstChildElement("lit") != nullptr;
	}
}

TEST_SUITE("Lit world UI")
{
	TEST_CASE("an element that asks to be lit is drawn lit under a world space canvas only")
	{
		OvCore::SceneSystem::Scene scene("", "");
		auto& canvasActor = scene.CreateActor("Canvas");
		auto& canvas = canvasActor.AddComponent<CCanvas>();

		ResolvedUIElement element;
		element.canvas = &canvas;
		element.canvasActor = &canvasActor;

		canvas.SetRenderMode(CCanvas::ERenderMode::WORLD_SPACE);
		CHECK(IsDrawnLit(true, &element));
		CHECK_FALSE(IsDrawnLit(false, &element));

		// A screen space canvas stays drawn over the scene, unlit, even previewed in the world by the editor
		canvas.SetRenderMode(CCanvas::ERenderMode::SCREEN_SPACE);
		element.screenSpace = true;
		CHECK_FALSE(IsDrawnLit(true, &element));
		element.screenSpace = false;
		CHECK_FALSE(IsDrawnLit(true, &element));

		// An element that could not be resolved, or is under no canvas
		const ResolvedUIElement outside;
		CHECK_FALSE(IsDrawnLit(true, nullptr));
		CHECK_FALSE(IsDrawnLit(true, &outside));
	}

	TEST_CASE("a text's lit setting is saved with it, and a text saved before it reads as unlit")
	{
		ProvideUIServices();

		OvCore::SceneSystem::Scene scene("", "");
		auto& text = scene.CreateActor("Text").AddComponent<CText>();
		auto& other = scene.CreateActor("Other").AddComponent<CText>();

		CHECK_FALSE(text.IsLit());

		text.SetLit(true);
		CHECK(text.IsLit());
		CHECK(SaveAndReadLit(text, other));
		CHECK(other.IsLit());

		text.SetLit(false);
		SaveAndReadLit(text, other);
		CHECK_FALSE(other.IsLit());

		// A file without the key leaves the setting as it is: a new text, unlit
		tinyxml2::XMLDocument older;
		auto* node = older.NewElement("data");
		older.InsertFirstChild(node);
		auto* textNode = older.NewElement("text");
		textNode->SetText("Older");
		node->InsertEndChild(textNode);

		auto& fresh = scene.CreateActor("Fresh").AddComponent<CText>();
		fresh.OnDeserialize(older, node);
		CHECK(fresh.GetText() == "Older");
		CHECK_FALSE(fresh.IsLit());
	}

	TEST_CASE("without a lit material from the host, a lit text has none, and is drawn with its own")
	{
		ProvideUIServices();

		OvCore::SceneSystem::Scene scene("", "");
		auto& text = scene.CreateActor("Text").AddComponent<CText>();
		text.SetLit(true);

		CHECK(text.GetLitMaterial() == nullptr);
		CHECK(text.GetLitMaterial() == nullptr);
	}
}
