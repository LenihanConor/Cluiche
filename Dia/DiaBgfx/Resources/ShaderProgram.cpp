////////////////////////////////////////////////////////////////////////////////
// Filename: ShaderProgram.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Resources/ShaderProgram.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

#include <bgfx/bgfx.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    bgfx::ShaderHandle LoadShaderFile(const char* filePath)
    {
        FILE* f = nullptr;
        fopen_s(&f, filePath, "rb");
        if (!f)
        {
            DIA_LOG_ERROR("DiaBgfx", "ShaderProgram: cannot open '%s'", filePath);
            return BGFX_INVALID_HANDLE;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0)
        {
            fclose(f);
            DIA_LOG_ERROR("DiaBgfx", "ShaderProgram: empty file '%s'", filePath);
            return BGFX_INVALID_HANDLE;
        }

        const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
        fread(mem->data, 1, static_cast<size_t>(size), f);
        mem->data[size] = '\0';
        fclose(f);

        bgfx::ShaderHandle sh = bgfx::createShader(mem);
        if (!bgfx::isValid(sh))
        {
            DIA_LOG_ERROR("DiaBgfx", "ShaderProgram: bgfx failed to create shader from '%s'", filePath);
        }
        return sh;
    }
}

namespace Dia
{
    namespace Bgfx
    {
        ShaderProgram::ShaderProgram()
            : mVS(bgfx::kInvalidHandle)
            , mFS(bgfx::kInvalidHandle)
            , mProgram(bgfx::kInvalidHandle)
        {}

        ShaderProgram::~ShaderProgram()
        {
            if (bgfx::isValid(bgfx::ProgramHandle{ mProgram }))
                bgfx::destroy(bgfx::ProgramHandle{ mProgram });
            // bgfx destroys attached shaders when the program is destroyed
            mProgram = bgfx::kInvalidHandle;
            mVS      = bgfx::kInvalidHandle;
            mFS      = bgfx::kInvalidHandle;
        }

        bool ShaderProgram::Load(const char* cookedShaderRoot,
                                 const char* backendSubdir,
                                 const char* vsName,
                                 const char* fsName)
        {
            DIA_ASSERT(cookedShaderRoot != nullptr, "ShaderProgram::Load: cookedShaderRoot is null");
            DIA_ASSERT(backendSubdir != nullptr,    "ShaderProgram::Load: backendSubdir is null");
            DIA_ASSERT(vsName != nullptr,           "ShaderProgram::Load: vsName is null");
            DIA_ASSERT(fsName != nullptr,           "ShaderProgram::Load: fsName is null");

            char vsPath[512];
            char fsPath[512];
            snprintf(vsPath, sizeof(vsPath), "%s/%s/vs_%s.bin", cookedShaderRoot, backendSubdir, vsName);
            snprintf(fsPath, sizeof(fsPath), "%s/%s/fs_%s.bin", cookedShaderRoot, backendSubdir, fsName);

            bgfx::ShaderHandle vs = LoadShaderFile(vsPath);
            if (!bgfx::isValid(vs))
                return false;

            bgfx::ShaderHandle fs = LoadShaderFile(fsPath);
            if (!bgfx::isValid(fs))
            {
                bgfx::destroy(vs);
                return false;
            }

            bgfx::ProgramHandle prog = bgfx::createProgram(vs, fs, true /* destroyShaders */);
            if (!bgfx::isValid(prog))
            {
                DIA_LOG_ERROR("DiaBgfx", "ShaderProgram::Load: bgfx::createProgram failed for %s/%s", vsName, fsName);
                return false;
            }

            mVS      = vs.idx;
            mFS      = fs.idx;
            mProgram = prog.idx;
            return true;
        }

        bool ShaderProgram::LoadFromPath(const char* cookedShaderRoot,
                                         const char* backendSubdir,
                                         const char* vsRelBin,
                                         const char* fsRelBin)
        {
            DIA_ASSERT(cookedShaderRoot != nullptr, "ShaderProgram::LoadFromPath: cookedShaderRoot is null");
            DIA_ASSERT(backendSubdir != nullptr,    "ShaderProgram::LoadFromPath: backendSubdir is null");
            DIA_ASSERT(vsRelBin != nullptr,         "ShaderProgram::LoadFromPath: vsRelBin is null");
            DIA_ASSERT(fsRelBin != nullptr,         "ShaderProgram::LoadFromPath: fsRelBin is null");

            char vsPath[512];
            char fsPath[512];
            snprintf(vsPath, sizeof(vsPath), "%s/%s/%s", cookedShaderRoot, backendSubdir, vsRelBin);
            snprintf(fsPath, sizeof(fsPath), "%s/%s/%s", cookedShaderRoot, backendSubdir, fsRelBin);

            bgfx::ShaderHandle vs = LoadShaderFile(vsPath);
            if (!bgfx::isValid(vs))
                return false;

            bgfx::ShaderHandle fs = LoadShaderFile(fsPath);
            if (!bgfx::isValid(fs))
            {
                bgfx::destroy(vs);
                return false;
            }

            bgfx::ProgramHandle prog = bgfx::createProgram(vs, fs, true /* destroyShaders */);
            if (!bgfx::isValid(prog))
            {
                DIA_LOG_ERROR("DiaBgfx", "ShaderProgram::LoadFromPath: bgfx::createProgram failed for %s / %s", vsRelBin, fsRelBin);
                return false;
            }

            mVS      = vs.idx;
            mFS      = fs.idx;
            mProgram = prog.idx;
            DIA_LOG_INFO("DiaBgfx", "ShaderProgram::LoadFromPath: loaded %s / %s", vsRelBin, fsRelBin);
            return true;
        }

        bool ShaderProgram::IsValid() const
        {
            return bgfx::isValid(bgfx::ProgramHandle{ mProgram });
        }

    } // namespace Bgfx
} // namespace Dia
