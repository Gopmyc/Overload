/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>
#include <utility>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

#include <OvDebug/Logger.h>
#include <OvPhysics/Entities/PhysicalMesh.h>

namespace
{
	constexpr float kDefaultFriction = 0.8f;
}

/**
* The last shape made, kept for as long as the triangles and the scale stay the same, so that recreating the body
* (when it changes settings) doesn't rebuild the mesh's tree
*/
struct OvPhysics::Entities::PhysicalMesh::ShapeCache
{
	JPH::Ref<JPH::Shape> shape;
	OvMaths::FVector3 scale;
};

OvPhysics::Entities::PhysicalMesh::PhysicalMesh() :
	PhysicalObject(),
	m_shapeCache(std::make_unique<ShapeCache>())
{
	Init();
	SetFriction(kDefaultFriction);
}

OvPhysics::Entities::PhysicalMesh::PhysicalMesh(OvMaths::FTransform& p_transform) :
	PhysicalObject(p_transform),
	m_shapeCache(std::make_unique<ShapeCache>())
{
	Init();
	SetFriction(kDefaultFriction);
}

OvPhysics::Entities::PhysicalMesh::~PhysicalMesh() = default;

bool OvPhysics::Entities::PhysicalMesh::SetTriangles(std::vector<float> p_positions, std::vector<uint32_t> p_indices)
{
	if (p_positions.size() % 3 != 0 || p_indices.size() % 3 != 0)
	{
		return false;
	}

	const size_t vertexCount = p_positions.size() / 3;

	for (const uint32_t index : p_indices)
	{
		if (index >= vertexCount)
		{
			return false;
		}
	}

	m_positions = std::move(p_positions);
	m_indices = std::move(p_indices);
	m_shapeCache->shape = nullptr;

	RecreateBody();
	return true;
}

uint32_t OvPhysics::Entities::PhysicalMesh::GetTriangleCount() const
{
	return static_cast<uint32_t>(m_indices.size() / 3);
}

bool OvPhysics::Entities::PhysicalMesh::MustBeStatic() const
{
	return true;
}

JPH::Shape* OvPhysics::Entities::PhysicalMesh::CreateShape(const OvMaths::FVector3& p_scale) const
{
	if (m_indices.empty())
	{
		return new JPH::EmptyShape();
	}

	if (m_shapeCache->shape && m_shapeCache->scale.x == p_scale.x && m_shapeCache->scale.y == p_scale.y && m_shapeCache->scale.z == p_scale.z)
	{
		return m_shapeCache->shape.GetPtr();
	}

	JPH::VertexList vertices;
	vertices.reserve(m_positions.size() / 3);

	for (size_t index = 0; index + 2 < m_positions.size(); index += 3)
	{
		vertices.push_back(JPH::Float3(
			m_positions[index] * p_scale.x,
			m_positions[index + 1] * p_scale.y,
			m_positions[index + 2] * p_scale.z
		));
	}

	JPH::IndexedTriangleList triangles;
	triangles.reserve(m_indices.size() / 3);

	for (size_t index = 0; index + 2 < m_indices.size(); index += 3)
	{
		triangles.push_back(JPH::IndexedTriangle(m_indices[index], m_indices[index + 1], m_indices[index + 2]));
	}

	JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
	const JPH::ShapeSettings::ShapeResult result = settings.Create();

	if (result.HasError())
	{
		OVLOG_ERROR(std::string("Unable to create a physical mesh: ") + result.GetError().c_str());
		return new JPH::EmptyShape();
	}

	m_shapeCache->shape = result.Get();
	m_shapeCache->scale = p_scale;

	return m_shapeCache->shape.GetPtr();
}
