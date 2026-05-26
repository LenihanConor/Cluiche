////////////////////////////////////////////////////////////////////////////////
// Filename: BgfxImGuiRenderer.h
// Description: bgfx-side ImGui renderer. Converts ImDrawData into bgfx draw
//              calls. Compatible with Dear ImGui 1.90.x.
//              Exposes imguiCreate/imguiDestroy/imguiBeginFrame/imguiEndFrame.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <bgfx/bgfx.h>

namespace bx { struct AllocatorI; }

void imguiCreate(float fontSize = 18.0f, bx::AllocatorI* allocator = nullptr);
void imguiDestroy();

void imguiBeginFrame(int32_t mx, int32_t my, uint8_t button, int32_t scroll,
                     uint16_t width, uint16_t height,
                     int inputChar = -1, bgfx::ViewId view = 255);
void imguiEndFrame();

#endif // DIA_DEBUG
