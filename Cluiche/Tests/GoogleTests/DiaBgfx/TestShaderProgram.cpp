// TestShaderProgram.cpp - Google Test unit tests for ShaderProgram
//
// Only the paths that do NOT require a live bgfx context are tested:
//   - default-constructed program is invalid
//   - Load / LoadFromPath against a missing file return false (the file open
//     fails before any bgfx::createShader call is reached)
//
// The success path (real .bin -> bgfx::createProgram) needs a GPU/bgfx context
// and is covered by the visual cluichetest run, not here. The value of these
// tests is the path-construction + early-return contract, especially that
// LoadFromPath does NOT prepend vs_/fs_ (unlike Load).

#include <gtest/gtest.h>
#include <DiaBgfx/Resources/ShaderProgram.h>

using Dia::Bgfx::ShaderProgram;

TEST(DiaBgfx_ShaderProgramTest, DefaultConstructedIsInvalid)
{
    ShaderProgram prog;
    EXPECT_FALSE(prog.IsValid());
}

TEST(DiaBgfx_ShaderProgramTest, LoadMissingFileReturnsFalse)
{
    ShaderProgram prog;
    // Load() prepends vs_/fs_ -> looks for <root>/<backend>/vs_nope.bin etc.
    bool ok = prog.Load("C:/nonexistent/shaders", "dx11", "nope", "nope");
    EXPECT_FALSE(ok);
    EXPECT_FALSE(prog.IsValid());
}

TEST(DiaBgfx_ShaderProgramTest, LoadFromPathMissingFileReturnsFalse)
{
    ShaderProgram prog;
    // LoadFromPath() uses the relative path verbatim (no vs_/fs_ prefix) ->
    // <root>/<backend>/3d/vs_mesh.bin. Missing file => false, stays invalid.
    bool ok = prog.LoadFromPath("C:/nonexistent/shaders", "dx11",
                                "3d/vs_mesh.bin", "3d/fs_mesh.bin");
    EXPECT_FALSE(ok);
    EXPECT_FALSE(prog.IsValid());
}

TEST(DiaBgfx_ShaderProgramTest, LoadFromPathMissingVertexShaderShortCircuits)
{
    // Even when the fragment path is plausible, a missing vertex .bin must
    // fail the whole load (vs is loaded first; fs is never reached).
    ShaderProgram prog;
    bool ok = prog.LoadFromPath("C:/nonexistent/shaders", "vulkan",
                                "3d/vs_shadow_caster.bin", "3d/fs_shadow_caster.bin");
    EXPECT_FALSE(ok);
    EXPECT_FALSE(prog.IsValid());
}
