/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/Rendering/UIRenderingUtils.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvMaths/FVector4.h>

using OvCore::Rendering::UIRenderingUtils::CreateUIClip;
using OvCore::Rendering::UIRenderingUtils::IsOutsideUIClip;
using OvCore::Rendering::UIRenderingUtils::ResolvedUIElement;
using OvCore::Rendering::UIRenderingUtils::UIClip;
using OvMaths::FMatrix4;
using OvMaths::FVector2;
using OvMaths::FVector4;

namespace
{
	/**
	* An element of a canvas: its centre and size in canvas units, turned by an angle, the canvas placed by a matrix
	*/
	ResolvedUIElement Element(FVector2 p_center, FVector2 p_size, float p_angle = 0.0f, const FMatrix4& p_canvas = FMatrix4::Identity)
	{
		ResolvedUIElement element;
		element.canvasMatrix = p_canvas;
		element.frameMatrix = p_canvas * FMatrix4::Translation({ p_center.x, p_center.y, 0.0f }) * FMatrix4::RotationOnAxisZ(p_angle);
		element.effectiveSize = p_size;
		return element;
	}

	/**
	* Where a point of the element's own frame lands in the clip's [-1, 1] space
	*/
	FVector2 InClip(const UIClip& p_clip, const ResolvedUIElement& p_element, FVector2 p_point)
	{
		const FVector4 point = p_clip.matrix * (p_element.frameMatrix * FVector4{ p_point.x, p_point.y, 0.0f, 1.0f });
		return { point.x, point.y };
	}
}

TEST_SUITE("UI clip")
{
	TEST_CASE("an upright element clips to its rectangle of the canvas")
	{
		const auto panel = Element({ 300.0f, 200.0f }, { 200.0f, 100.0f });
		const UIClip clip = CreateUIClip(panel, nullptr);

		CHECK(clip.upright);
		CHECK_FALSE(clip.empty);
		CHECK(clip.min.x == doctest::Approx(200.0f));
		CHECK(clip.min.y == doctest::Approx(150.0f));
		CHECK(clip.max.x == doctest::Approx(400.0f));
		CHECK(clip.max.y == doctest::Approx(250.0f));

		// Its corners are the clip's corners, its centre the clip's centre
		CHECK(InClip(clip, panel, { 100.0f, 50.0f }).x == doctest::Approx(1.0f));
		CHECK(InClip(clip, panel, { 100.0f, 50.0f }).y == doctest::Approx(1.0f));
		CHECK(InClip(clip, panel, { -100.0f, -50.0f }).x == doctest::Approx(-1.0f));
		CHECK(InClip(clip, panel, { 0.0f, 0.0f }).x == doctest::Approx(0.0f).epsilon(1e-4));
		CHECK(std::abs(InClip(clip, panel, { 150.0f, 0.0f }).x) > 1.0f);
	}

	TEST_CASE("a clip inside a clip lets through what both do, and nothing when they don't meet")
	{
		const UIClip outer = CreateUIClip(Element({ 300.0f, 200.0f }, { 200.0f, 100.0f }), nullptr);
		const auto inner = Element({ 380.0f, 240.0f }, { 100.0f, 100.0f });
		const UIClip both = CreateUIClip(inner, &outer);

		CHECK(both.upright);
		CHECK(both.min.x == doctest::Approx(330.0f));
		CHECK(both.min.y == doctest::Approx(190.0f));
		CHECK(both.max.x == doctest::Approx(400.0f));
		CHECK(both.max.y == doctest::Approx(250.0f));

		const UIClip apart = CreateUIClip(Element({ 900.0f, 900.0f }, { 50.0f, 50.0f }), &outer);
		CHECK(apart.empty);

		// What an empty clip holds is empty too
		CHECK(CreateUIClip(Element({ 0.0f, 0.0f }, { 10.0f, 10.0f }), &apart).empty);
		CHECK(CreateUIClip(Element({ 0.0f, 0.0f }, { 0.0f, 10.0f }), nullptr).empty);
	}

	TEST_CASE("a turned element clips to its own rectangle, in its own frame")
	{
		const auto turned = Element({ 300.0f, 200.0f }, { 200.0f, 100.0f }, 0.5f);
		const UIClip clip = CreateUIClip(turned, nullptr);

		CHECK_FALSE(clip.upright);
		CHECK_FALSE(clip.empty);
		CHECK(InClip(clip, turned, { 100.0f, 50.0f }).x == doctest::Approx(1.0f));
		CHECK(InClip(clip, turned, { 100.0f, 50.0f }).y == doctest::Approx(1.0f));
		CHECK(InClip(clip, turned, { -100.0f, 0.0f }).x == doctest::Approx(-1.0f));

		// Inside a turned clip, an upright one keeps its own rectangle: the innermost cuts
		const auto upright = Element({ 300.0f, 200.0f }, { 50.0f, 50.0f });
		const UIClip inner = CreateUIClip(upright, &clip);
		CHECK_FALSE(inner.upright);
		CHECK(InClip(inner, upright, { 25.0f, 25.0f }).x == doctest::Approx(1.0f));
	}

	TEST_CASE("on a world space canvas, the clip is in canvas units and maps world points")
	{
		const FMatrix4 canvas = FMatrix4::Translation({ 0.0f, 1.5f, 3.0f }) * FMatrix4::RotationOnAxisY(0.7f) * FMatrix4::Scaling({ 0.002f, 0.002f, 0.002f });
		const auto panel = Element({ 100.0f, 50.0f }, { 200.0f, 100.0f }, 0.0f, canvas);
		const UIClip clip = CreateUIClip(panel, nullptr);

		CHECK(clip.upright);
		CHECK(clip.min.x == doctest::Approx(0.0f).epsilon(1e-3));
		CHECK(clip.max.x == doctest::Approx(200.0f).epsilon(1e-3));
		CHECK(InClip(clip, panel, { 100.0f, 50.0f }).x == doctest::Approx(1.0f).epsilon(1e-3));
		CHECK(InClip(clip, panel, { -100.0f, -50.0f }).y == doctest::Approx(-1.0f).epsilon(1e-3));
	}

	TEST_CASE("an element entirely outside an upright clip is known to be")
	{
		const UIClip clip = CreateUIClip(Element({ 300.0f, 200.0f }, { 200.0f, 100.0f }), nullptr);

		CHECK(IsOutsideUIClip(clip, Element({ 600.0f, 200.0f }, { 100.0f, 100.0f })));
		CHECK(IsOutsideUIClip(clip, Element({ 300.0f, -50.0f }, { 100.0f, 100.0f })));
		CHECK_FALSE(IsOutsideUIClip(clip, Element({ 420.0f, 200.0f }, { 100.0f, 100.0f })));

		// A turned clip cannot tell
		const UIClip turned = CreateUIClip(Element({ 300.0f, 200.0f }, { 200.0f, 100.0f }, 0.5f), nullptr);
		CHECK_FALSE(IsOutsideUIClip(turned, Element({ 900.0f, 900.0f }, { 10.0f, 10.0f })));
	}

	TEST_CASE("the clip is saved with the UI transform, and only when set")
	{
		OvCore::SceneSystem::Scene scene("", "");
		auto& actor = scene.CreateActor("Panel");
		actor.transform.EnableUIData();

		const auto save = [&actor]()
		{
			tinyxml2::XMLDocument document;
			auto* node = document.NewElement("transform");
			document.InsertFirstChild(node);
			actor.transform.OnSerialize(document, node);
			return node->FirstChildElement("ui_clip_children") != nullptr;
		};

		CHECK_FALSE(actor.transform.GetUIClipChildren());
		CHECK_FALSE(save());

		actor.transform.SetUIClipChildren(true);
		CHECK(save());

		// Read back into another element
		tinyxml2::XMLDocument document;
		auto* node = document.NewElement("transform");
		document.InsertFirstChild(node);
		actor.transform.OnSerialize(document, node);

		auto& other = scene.CreateActor("Other");
		other.transform.OnDeserialize(document, node);
		CHECK(other.transform.HasUIData());
		CHECK(other.transform.GetUIClipChildren());

		// A scene saved before the clip existed reads as not clipping
		actor.transform.SetUIClipChildren(false);
		tinyxml2::XMLDocument older;
		auto* olderNode = older.NewElement("transform");
		older.InsertFirstChild(olderNode);
		actor.transform.OnSerialize(older, olderNode);
		other.transform.OnDeserialize(older, olderNode);
		CHECK_FALSE(other.transform.GetUIClipChildren());
	}
}
