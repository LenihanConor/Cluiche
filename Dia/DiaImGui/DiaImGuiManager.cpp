////////////////////////////////////////////////////////////////////////////////
// Filename: DiaImGuiManager.cpp
// Description: Implementation of DiaImGuiManager and free functions
////////////////////////////////////////////////////////////////////////////////
#include "DiaImGui/DiaImGuiManager.h"
#include "DiaImGui/IImGuiBackend.h"

namespace Dia
{
    namespace ImGui
    {
        // -----------------------------------------------------------------
        // DiaImGuiManager methods
        // -----------------------------------------------------------------

        void DiaImGuiManager::SetBackend(IImGuiBackend* backend)
        {
            mBackend = backend;
        }

        IImGuiBackend* DiaImGuiManager::GetBackend() const
        {
            return mBackend;
        }

        void DiaImGuiManager::Init()
        {
            if (mBackend)
                mBackend->Init();
        }

        void DiaImGuiManager::Shutdown()
        {
            if (mBackend)
                mBackend->Shutdown();
        }

        void DiaImGuiManager::NewFrame(float dt)
        {
            if (mBackend)
                mBackend->NewFrame(dt);
        }

        void DiaImGuiManager::Render()
        {
            if (mBackend)
                mBackend->Render();
        }

        // -----------------------------------------------------------------
        // Global instance and free functions
        // -----------------------------------------------------------------

        static DiaImGuiManager sManager;

        DiaImGuiManager& GetManager()
        {
            return sManager;
        }

        void SetBackend(IImGuiBackend* backend)
        {
            sManager.SetBackend(backend);
        }

        void Init()
        {
            sManager.Init();
        }

        void Shutdown()
        {
            sManager.Shutdown();
        }

        void NewFrame(float dt)
        {
            sManager.NewFrame(dt);
        }

        void Render()
        {
            sManager.Render();
        }

    } // namespace ImGui
} // namespace Dia
