/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include "OvRendering/Data/PipelineState.h"

OvRendering::Data::PipelineState::PipelineState() :
	colorWriting{true, true, true, true},
	depthWriting(true),
	culling(true),
	dither(false),
	polygonOffsetFill(false),
	sampleAlphaToCoverage(false),
	depthTest(true),
	scissorTest(false),
	stencilTest(false),
	multisample(true),
	rasterizationMode(baregl::types::ERasterizationMode::FILL),
	stencilFuncOp(baregl::types::EComparaisonAlgorithm::ALWAYS),
	stencilFuncRef(0x00),
	stencilFuncMask(0xFF),
	stencilWriteMask(0xFF),
	stencilOpFail(baregl::types::EOperation::KEEP),
	depthOpFail(baregl::types::EOperation::KEEP),
	bothOpFail(baregl::types::EOperation::KEEP),
	depthFunc(baregl::types::EComparaisonAlgorithm::LESS),
	cullFace(baregl::types::ECullFace::BACK),
	lineWidthPow2(0x00),
	blending(false),
	blendingSrcFactor(baregl::types::EBlendingFactor::SRC_ALPHA),
	blendingEquation(baregl::types::EBlendingEquation::FUNC_ADD),
	blendingDestFactor(baregl::types::EBlendingFactor::ONE_MINUS_SRC_ALPHA)
{
}

void OvRendering::Data::PipelineState::ApplyBlendMode(Settings::EBlendMode p_blendMode)
{
	using namespace baregl::types;

	switch (p_blendMode)
	{
	case Settings::EBlendMode::ADDITIVE:
		blendingSrcFactor = EBlendingFactor::SRC_ALPHA;
		blendingDestFactor = EBlendingFactor::ONE;
		blendingEquation = EBlendingEquation::FUNC_ADD;
		break;
	case Settings::EBlendMode::PREMULTIPLIED:
		blendingSrcFactor = EBlendingFactor::ONE;
		blendingDestFactor = EBlendingFactor::ONE_MINUS_SRC_ALPHA;
		blendingEquation = EBlendingEquation::FUNC_ADD;
		break;
	case Settings::EBlendMode::ALPHA:
	default:
		break;
	}
}
