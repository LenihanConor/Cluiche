#pragma once
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <mutex>

namespace Dia
{
    namespace Input
    {
        enum class EInputRouting { kGameOnly, kUIOnly, kGameAndUI };

        // Arbitrates keyboard routing between game and in-game UI panels.
        // Owned by UIModule; injected into UltralightUISystem via SetInputRouter().
        // Stack default (empty) = kGameOnly.
        class InputRouter
        {
        public:
            void PushInputMode(EInputRouting mode)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mStack.IsFull())
                    mStack.Add(mode);
            }

            void PopInputMode()
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mStack.IsEmpty())
                    mStack.Remove();
            }

            [[nodiscard]] EInputRouting GetCurrentInputMode() const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mStack.IsEmpty() ? EInputRouting::kGameOnly : mStack.Back();
            }

        private:
            Dia::Core::Containers::DynamicArrayC<EInputRouting, 8> mStack;
            mutable std::mutex mMutex;
        };

    } // namespace Input
} // namespace Dia
