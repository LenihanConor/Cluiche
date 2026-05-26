////////////////////////////////////////////////////////////////////////////////
// Filename: BgfxImGuiRenderer.cpp
// Description: bgfx ImGui renderer — compatible with Dear ImGui 1.90.x.
//              Font atlas upload and ImDrawData -> bgfx draw call translation.
//              Based on bgfx examples/common/imgui/imgui.cpp (MIT license),
//              rewritten for imgui 1.90.x API (no ImTextureData / DockContext).
////////////////////////////////////////////////////////////////////////////////

#ifdef DIA_DEBUG

#include "DiaBgfx/Imgui/BgfxImGuiRenderer.h"

#include <imgui.h>

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/allocator.h>
#include <bx/math.h>
#include <bx/timer.h>

// Pre-compiled embedded shaders from bgfx examples/common/imgui/
#include "../../../External/bgfx/examples/common/imgui/vs_ocornut_imgui.bin.h"
#include "../../../External/bgfx/examples/common/imgui/fs_ocornut_imgui.bin.h"
#include "../../../External/bgfx/examples/common/imgui/vs_imgui_image.bin.h"
#include "../../../External/bgfx/examples/common/imgui/fs_imgui_image.bin.h"

// Embedded font TTFs (from bgfx upstream — MIT licensed)
#include "../../../External/bgfx/examples/common/imgui/roboto_regular.ttf.h"
#include "../../../External/bgfx/examples/common/imgui/robotomono_regular.ttf.h"
#include "../../../External/bgfx/examples/common/imgui/icons_kenney.ttf.h"
#include "../../../External/bgfx/examples/common/imgui/icons_font_awesome.ttf.h"

// Icon font range headers
#include "../../../External/bgfx/3rdparty/iconfontheaders/icons_kenney.h"
#include "../../../External/bgfx/3rdparty/iconfontheaders/icons_font_awesome.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(vs_imgui_image),
    BGFX_EMBEDDED_SHADER(fs_imgui_image),
    BGFX_EMBEDDED_SHADER_END()
};

struct FontRangeMerge
{
    const void* data;
    size_t      size;
    ImWchar     ranges[3];
};

static FontRangeMerge s_fontRangeMerge[] =
{
    { s_iconsKenneyTtf,      sizeof(s_iconsKenneyTtf),      { ICON_MIN_KI, ICON_MAX_KI, 0 } },
    { s_iconsFontAwesomeTtf, sizeof(s_iconsFontAwesomeTtf), { ICON_MIN_FA, ICON_MAX_FA, 0 } },
};

struct BgfxImGuiContext
{
    bgfx::VertexLayout  m_layout;
    bgfx::ProgramHandle m_program      = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_imageProgram = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_texture      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex          = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_imageLodEnabled = BGFX_INVALID_HANDLE;
    bgfx::ViewId        m_viewId       = 255;
    int64_t             m_last         = 0;
    int32_t             m_lastScroll   = 0;
    bx::AllocatorI*     m_allocator    = nullptr;

    void render(ImDrawData* drawData)
    {
        int32_t dispWidth  = int32_t(drawData->DisplaySize.x * drawData->FramebufferScale.x);
        int32_t dispHeight = int32_t(drawData->DisplaySize.y * drawData->FramebufferScale.y);
        if (dispWidth <= 0 || dispHeight <= 0)
            return;

        bgfx::setViewName(m_viewId, "ImGui");
        bgfx::setViewMode(m_viewId, bgfx::ViewMode::Sequential);

        const bgfx::Caps* caps = bgfx::getCaps();
        {
            float ortho[16];
            float x      = drawData->DisplayPos.x;
            float y      = drawData->DisplayPos.y;
            float width  = drawData->DisplaySize.x;
            float height = drawData->DisplaySize.y;
            bx::mtxOrtho(ortho, x, x + width, y + height, y, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
            bgfx::setViewTransform(m_viewId, nullptr, ortho);
            bgfx::setViewRect(m_viewId, 0, 0, uint16_t(width), uint16_t(height));
        }

        const ImVec2 clipPos   = drawData->DisplayPos;
        const ImVec2 clipScale = drawData->FramebufferScale;

        for (int32_t ii = 0, num = drawData->CmdListsCount; ii < num; ++ii)
        {
            const ImDrawList* drawList = drawData->CmdLists[ii];
            uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
            uint32_t numIndices  = (uint32_t)drawList->IdxBuffer.size();

            if (numVertices == 0 || numIndices == 0)
                continue;

            if (!bgfx::getAvailTransientVertexBuffer(numVertices, m_layout) ||
                !bgfx::getAvailTransientIndexBuffer(numIndices, sizeof(ImDrawIdx) == 4))
                break;

            bgfx::TransientVertexBuffer tvb;
            bgfx::TransientIndexBuffer  tib;
            bgfx::allocTransientVertexBuffer(&tvb, numVertices, m_layout);
            bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);

            bx::memCopy(tvb.data, drawList->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));
            bx::memCopy(tib.data, drawList->IdxBuffer.begin(), numIndices  * sizeof(ImDrawIdx));

            bgfx::Encoder* encoder = bgfx::begin();

            for (const ImDrawCmd& cmd : drawList->CmdBuffer)
            {
                if (cmd.UserCallback)
                {
                    cmd.UserCallback(drawList, &cmd);
                    continue;
                }

                if (cmd.ElemCount == 0)
                    continue;

                uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;

                bgfx::TextureHandle th      = m_texture;
                bgfx::ProgramHandle program = m_program;

                if (cmd.TextureId != nullptr)
                {
                    // TextureId packs bgfx handle as pointer (stored by BgfxImGuiBackend::Init)
                    union { ImTextureID id; bgfx::TextureHandle handle; } tex;
                    tex.id = cmd.TextureId;
                    th = tex.handle;
                }

                state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);

                ImVec4 clipRect;
                clipRect.x = (cmd.ClipRect.x - clipPos.x) * clipScale.x;
                clipRect.y = (cmd.ClipRect.y - clipPos.y) * clipScale.y;
                clipRect.z = (cmd.ClipRect.z - clipPos.x) * clipScale.x;
                clipRect.w = (cmd.ClipRect.w - clipPos.y) * clipScale.y;

                if (clipRect.x <  dispWidth  &&
                    clipRect.y <  dispHeight &&
                    clipRect.z >= 0.0f        &&
                    clipRect.w >= 0.0f)
                {
                    const uint16_t xx = uint16_t(bx::max(clipRect.x, 0.0f));
                    const uint16_t yy = uint16_t(bx::max(clipRect.y, 0.0f));
                    encoder->setScissor(xx, yy,
                        uint16_t(bx::min(clipRect.z, 65535.0f) - xx),
                        uint16_t(bx::min(clipRect.w, 65535.0f) - yy));

                    encoder->setState(state);
                    encoder->setTexture(0, s_tex, th);
                    encoder->setVertexBuffer(0, &tvb, cmd.VtxOffset, numVertices);
                    encoder->setIndexBuffer(&tib, cmd.IdxOffset, cmd.ElemCount);
                    encoder->submit(m_viewId, program);
                }
            }

            bgfx::end(encoder);
        }
    }

    void create(float fontSize, bx::AllocatorI* allocator)
    {
        m_allocator = allocator;
        if (!m_allocator)
        {
            static bx::DefaultAllocator defaultAlloc;
            m_allocator = &defaultAlloc;
        }

        m_last       = bx::getHPCounter();
        m_lastScroll = 0;
        m_viewId     = 255;

        bgfx::RendererType::Enum type = bgfx::getRendererType();
        m_program = bgfx::createProgram(
            bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_ocornut_imgui"),
            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_ocornut_imgui"),
            true);

        u_imageLodEnabled = bgfx::createUniform("u_imageLodEnabled", bgfx::UniformType::Vec4);
        m_imageProgram = bgfx::createProgram(
            bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_imgui_image"),
            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_imgui_image"),
            true);

        m_layout
            .begin()
            .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
            .end();

        s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererName = "dia_bgfx";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

        // Load fonts
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        config.MergeMode = false;
        const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();
        io.Fonts->AddFontFromMemoryTTF((void*)s_robotoRegularTtf, sizeof(s_robotoRegularTtf), fontSize, &config, ranges);

        config.MergeMode = true;
        for (uint32_t ii = 0; ii < BX_COUNTOF(s_fontRangeMerge); ++ii)
        {
            const FontRangeMerge& frm = s_fontRangeMerge[ii];
            io.Fonts->AddFontFromMemoryTTF((void*)frm.data, (int)frm.size, fontSize - 3.0f, &config, frm.ranges);
        }

        // Upload font atlas to bgfx
        unsigned char* pixels;
        int atlasW, atlasH;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &atlasW, &atlasH);

        m_texture = bgfx::createTexture2D(
            (uint16_t)atlasW, (uint16_t)atlasH,
            false, 1,
            bgfx::TextureFormat::BGRA8,
            0,
            bgfx::copy(pixels, atlasW * atlasH * 4));
        bgfx::setName(m_texture, "ImGui Font Atlas");

        // Store handle in TexID as a plain pointer (reinterpreted on render)
        union { bgfx::TextureHandle h; ImTextureID id; } tex;
        tex.h = m_texture;
        io.Fonts->SetTexID(tex.id);
    }

    void destroy()
    {
        bgfx::destroy(s_tex);
        bgfx::destroy(u_imageLodEnabled);
        bgfx::destroy(m_imageProgram);
        bgfx::destroy(m_program);
        if (bgfx::isValid(m_texture))
            bgfx::destroy(m_texture);

        m_texture      = BGFX_INVALID_HANDLE;
        m_program      = BGFX_INVALID_HANDLE;
        m_imageProgram = BGFX_INVALID_HANDLE;
        s_tex          = BGFX_INVALID_HANDLE;
        m_allocator    = nullptr;
    }

    void beginFrame(int32_t mx, int32_t my, uint8_t button, int32_t scroll,
                    int width, int height, int inputChar, bgfx::ViewId viewId)
    {
        m_viewId = viewId;

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)width, (float)height);

        const int64_t now       = bx::getHPCounter();
        const int64_t frameTime = now - m_last;
        m_last = now;
        io.DeltaTime = float(frameTime / double(bx::getHPFrequency()));

        if (inputChar >= 0)
            io.AddInputCharacter(inputChar);

        io.AddMousePosEvent((float)mx, (float)my);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left,   0 != (button & 0x01));
        io.AddMouseButtonEvent(ImGuiMouseButton_Right,  0 != (button & 0x02));
        io.AddMouseButtonEvent(ImGuiMouseButton_Middle, 0 != (button & 0x04));
        io.AddMouseWheelEvent(0.0f, (float)(scroll - m_lastScroll));
        m_lastScroll = scroll;

        ImGui::NewFrame();
    }

    void endFrame()
    {
        ImGui::Render();
        render(ImGui::GetDrawData());
    }
};

static BgfxImGuiContext s_ctx;

void imguiCreate(float fontSize, bx::AllocatorI* allocator)
{
    s_ctx.create(fontSize, allocator);
}

void imguiDestroy()
{
    s_ctx.destroy();
}

void imguiBeginFrame(int32_t mx, int32_t my, uint8_t button, int32_t scroll,
                     uint16_t width, uint16_t height, int inputChar, bgfx::ViewId view)
{
    s_ctx.beginFrame(mx, my, button, scroll, width, height, inputChar, view);
}

void imguiEndFrame()
{
    s_ctx.endFrame();
}

#endif // DIA_DEBUG
