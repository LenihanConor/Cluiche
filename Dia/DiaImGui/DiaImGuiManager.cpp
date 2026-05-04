////////////////////////////////////////////////////////////////////////////////
// Filename: DiaImGuiManager.cpp
// Description: Implementation of DiaImGuiManager and free functions
////////////////////////////////////////////////////////////////////////////////
#include "DiaImGui/DiaImGuiManager.h"
#include "DiaImGui/IImGuiBackend.h"
#include <DiaCore/Core/Assert.h>

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
            DIA_ASSERT(mBackend != nullptr, "DiaImGuiManager::Init called with no backend set");
            mBackend->Init();
        }

        void DiaImGuiManager::Shutdown()
        {
            DIA_ASSERT(mBackend != nullptr, "DiaImGuiManager::Shutdown called with no backend set");
            mBackend->Shutdown();
        }

        void DiaImGuiManager::NewFrame(float dt)
        {
            DIA_ASSERT(mBackend != nullptr, "DiaImGuiManager::NewFrame called with no backend set");
            mBackend->NewFrame(dt);
        }

        void DiaImGuiManager::Render()
        {
            DIA_ASSERT(mBackend != nullptr, "DiaImGuiManager::Render called with no backend set");
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
