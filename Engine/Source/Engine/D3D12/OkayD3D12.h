#pragma once
#include "Engine/Okay.h"

#include <d3d12.h>
#include <dxgi1_4.h>

#define DX_CHECK(hResult) OKAY_ASSERT(SUCCEEDED(hResult))
#define D3D12_RELEASE(x) if (x) { x->Release(); x = nullptr; }0
