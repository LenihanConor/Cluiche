////////////////////////////////////////////////////////////////////////////////
// Filename: ShaderProgram.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
    namespace Bgfx
    {
        // Loads a cooked vs/fs pair from disk and wraps a bgfx program handle.
        // Lifetime: owned by Canvas; created at Initialize, destroyed at ~Canvas.
        class ShaderProgram
        {
        public:
            ShaderProgram();
            ~ShaderProgram();

            // Load cooked .bin files: <root>/<backendDir>/vs_<name>.bin + fs_<name>.bin
            // Returns false on missing file or bgfx shader/program creation failure.
            bool Load(const char* cookedShaderRoot,
                      const char* backendSubdir,
                      const char* vsName,
                      const char* fsName);

            // Load cooked .bin files using explicit relative paths: <root>/<backendDir>/<vsRelBin> + <root>/<backendDir>/<fsRelBin>
            // Does not prepend vs_/fs_ — caller supplies the full relative path (e.g. "3d/vs_mesh.bin")
            bool LoadFromPath(const char* cookedShaderRoot,
                              const char* backendSubdir,
                              const char* vsRelBin,
                              const char* fsRelBin);

            bool IsValid() const;
            unsigned short GetProgramHandle() const { return mProgram; }

        private:
            unsigned short mVS;
            unsigned short mFS;
            unsigned short mProgram;
        };

    } // namespace Bgfx
} // namespace Dia
