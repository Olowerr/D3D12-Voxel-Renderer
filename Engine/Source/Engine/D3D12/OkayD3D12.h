#pragma once
#include "Engine/Okay.h"

#include <d3dcompiler.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#define DX_CHECK(hResult) OKAY_ASSERT(SUCCEEDED(hResult))
#define D3D12_RELEASE(x) if (x) { x->Release(); x = nullptr; }0

namespace Okay
{
	inline const FilePath SHADER_PATH = FilePath("..") / "Engine" / "resources" / "shaders";;

	constexpr uint64_t alignUint64(uint64_t value, uint32_t alignment)
	{
		return ((value - 1) - ((value - 1) % alignment)) + alignment;
	}

	class ShaderIncludeReader : public ID3DInclude
	{
	public:
		// Inherited via ID3DInclude
		virtual HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
		{
			if (!Okay::readBinary(SHADER_PATH / pFileName, m_includeBuffer))
			{
				return E_FAIL;
			}

			*ppData = m_includeBuffer.c_str();
			*pBytes = (uint32_t)m_includeBuffer.size();

			return S_OK;
		}

		virtual HRESULT __stdcall Close(LPCVOID pData) override
		{
			m_includeBuffer.clear();
			m_includeBuffer.shrink_to_fit();
			return S_OK;
		}

	private:
		std::string m_includeBuffer;
	};

	inline D3D12_SHADER_BYTECODE compileShader(FilePath path, std::string_view version, ID3DBlob** pShaderBlob)
	{
		ID3DBlob* pErrorBlob = nullptr;

#ifdef _DEBUG
		uint32_t flags1 = D3DCOMPILE_DEBUG;
#else
		uint32_t flags1 = D3DCOMPILE_OPTIMIZATION_LEVEL2;
#endif

		ShaderIncludeReader includeReader;
		HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, &includeReader, "main", version.data(), flags1, 0, pShaderBlob, &pErrorBlob);

		if (FAILED(hr))
		{
			const char* pErrorMsg = pErrorBlob ? (const char*)pErrorBlob->GetBufferPointer() : "No errors produced, file might not have been found.";
			printf("Shader Compilation failed:\n%s\n", pErrorMsg);
			OKAY_ASSERT(false);
		}

		if (pErrorBlob)
		{
			const char* pErrorMsg = (const char*)pErrorBlob->GetBufferPointer();
			printf("Shader Compilation message:\n%s\n", pErrorMsg);
		}

		D3D12_RELEASE(pErrorBlob);

		D3D12_SHADER_BYTECODE shaderByteCode{};
		shaderByteCode.pShaderBytecode = (*pShaderBlob)->GetBufferPointer();
		shaderByteCode.BytecodeLength = (*pShaderBlob)->GetBufferSize();

		return shaderByteCode;
	}

	constexpr D3D12_ROOT_PARAMETER createRootParamCBV(D3D12_SHADER_VISIBILITY visibility, uint32_t shaderRegister, uint32_t registerSpace)
	{
		D3D12_ROOT_PARAMETER param = {};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		param.ShaderVisibility = visibility;
		param.Descriptor.ShaderRegister = shaderRegister;
		param.Descriptor.RegisterSpace = registerSpace;
		return param;
	}

	constexpr D3D12_ROOT_PARAMETER createRootParamSRV(D3D12_SHADER_VISIBILITY visibility, uint32_t shaderRegister, uint32_t registerSpace)
	{
		D3D12_ROOT_PARAMETER param = {};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		param.ShaderVisibility = visibility;
		param.Descriptor.ShaderRegister = shaderRegister;
		param.Descriptor.RegisterSpace = registerSpace;
		return param;
	}

	constexpr D3D12_BLEND_DESC createDefaultBlendDesc()
	{
		D3D12_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = false;
		desc.IndependentBlendEnable = false;

		D3D12_RENDER_TARGET_BLEND_DESC rtvBlendDesc = {};
		rtvBlendDesc.BlendEnable = false;
		rtvBlendDesc.LogicOpEnable = false;
		rtvBlendDesc.SrcBlend = D3D12_BLEND_ONE;
		rtvBlendDesc.DestBlend = D3D12_BLEND_ZERO;
		rtvBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		rtvBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		rtvBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
		rtvBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rtvBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
		rtvBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.RenderTarget[0] = rtvBlendDesc;
		desc.RenderTarget[1] = rtvBlendDesc;
		desc.RenderTarget[2] = rtvBlendDesc;
		desc.RenderTarget[3] = rtvBlendDesc;
		desc.RenderTarget[4] = rtvBlendDesc;
		desc.RenderTarget[5] = rtvBlendDesc;
		desc.RenderTarget[6] = rtvBlendDesc;
		desc.RenderTarget[7] = rtvBlendDesc;

		return desc;
	}

	constexpr D3D12_RASTERIZER_DESC createDefaultRasterizerDesc()
	{
		D3D12_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_BACK;
		desc.FrontCounterClockwise = false;
		desc.DepthBias = 0;
		desc.DepthBiasClamp = 0.0f;
		desc.SlopeScaledDepthBias = 0.0f;
		desc.DepthClipEnable = true;
		desc.MultisampleEnable = false;
		desc.AntialiasedLineEnable = false;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		return desc;
	}

	constexpr D3D12_DEPTH_STENCIL_DESC createDefaultDepthStencilDesc()
	{
		D3D12_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

		desc.StencilEnable = false;
		desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		return desc;
	}

	constexpr D3D12_GRAPHICS_PIPELINE_STATE_DESC createDefaultGraphicsPipelineStateDesc()
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.StreamOutput.pSODeclaration = nullptr;
		desc.StreamOutput.NumEntries = 0;
		desc.StreamOutput.pBufferStrides = nullptr;
		desc.StreamOutput.NumStrides = 0;
		desc.StreamOutput.RasterizedStream = 0;

		desc.BlendState = createDefaultBlendDesc();
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		desc.RasterizerState = createDefaultRasterizerDesc();
		desc.DepthStencilState = createDefaultDepthStencilDesc();

		desc.InputLayout.pInputElementDescs = nullptr;
		desc.InputLayout.NumElements = 0;
		desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		desc.NumRenderTargets = 0;
		desc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		desc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
		desc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;
		desc.RTVFormats[3] = DXGI_FORMAT_UNKNOWN;
		desc.RTVFormats[4] = DXGI_FORMAT_UNKNOWN;
		desc.RTVFormats[5] = DXGI_FORMAT_UNKNOWN;
		desc.RTVFormats[6] = DXGI_FORMAT_UNKNOWN;
		desc.RTVFormats[7] = DXGI_FORMAT_UNKNOWN;

		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		desc.NodeMask = 0;

		desc.CachedPSO.CachedBlobSizeInBytes = 0;
		desc.CachedPSO.pCachedBlob = nullptr;

		desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		return desc;
	}
}