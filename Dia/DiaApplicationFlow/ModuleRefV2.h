#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>

// ProcessingUnit is forward-declared here.  ModuleRef<T>::Get() calls
// pu->FindModule(), so any translation unit that instantiates Get() must
// include the ProcessingUnit header BEFORE including this file (or after,
// as long as the full definition is visible at the point of instantiation).
// This is idiomatic for template headers that depend on incomplete types.
namespace Dia { namespace ApplicationFlow { class ProcessingUnit; }}

namespace Dia { namespace ApplicationFlow {

    // ---------------------------------------------------------------------------
    // ModuleRef<T>
    //
    // Lifecycle-safe handle for accessing a sibling Module within the same
    // ProcessingUnit.  It is the SOLE inter-module access pattern in v2.
    //
    // Usage:
    //   // In module declaration:
    //   ModuleRef<PhysicsModule> mPhysics{this};
    //
    //   // In DoUpdate:
    //   if (auto* phys = mPhysics.Get()) { phys->Step(dt); }
    //
    // Resolution is lazy: the first call to Get() queries the owning PU's
    // module list.  The result is cached as long as the found module remains
    // kActive; the cache is invalidated on any subsequent call where the
    // cached module is no longer kActive.
    //
    // If the target module is absent or not kActive, Get() returns nullptr and
    // operator bool() evaluates to false.
    // ---------------------------------------------------------------------------
    template<typename T>
    class ModuleRef
    {
    public:
        // instanceId defaults to T::kTypeId.
        // Pass an explicit instanceId when multiple instances of the same type
        // coexist in the same ProcessingUnit.
        explicit ModuleRef(Module* owner,
                           const Dia::Core::StringCRC& instanceId = T::kTypeId);

        // Returns the target module if it is kActive, nullptr otherwise.
        T*       Get();
        const T* Get() const;

        // Convenience dereference — asserts if Get() would return nullptr.
        T*       operator->();
        const T* operator->() const;

        // Evaluates true when Get() != nullptr (i.e. target is kActive).
        explicit operator bool() const;

    private:
        Module*              mOwner;
        Dia::Core::StringCRC mTargetId;
        mutable T*           mCached = nullptr;
    };

    //--------------------------------------------------------------------------
    // Inline implementation (template — must remain in header)
    //--------------------------------------------------------------------------

    template<typename T>
    ModuleRef<T>::ModuleRef(Module* owner, const Dia::Core::StringCRC& instanceId)
        : mOwner(owner)
        , mTargetId(instanceId)
        , mCached(nullptr)
    {
        DIA_ASSERT(mOwner != nullptr, "ModuleRef: owner cannot be null");
    }

    template<typename T>
    T* ModuleRef<T>::Get()
    {
        // Validate the cached pointer: only reuse it while the target is active.
        if (mCached != nullptr && mCached->GetState() == ModuleState::kActive)
            return mCached;

        mCached = nullptr;

        ProcessingUnit* pu = mOwner->GetProcessingUnit();
        if (pu == nullptr)
            return nullptr;

        Module* found = pu->FindModule(mTargetId);
        if (found != nullptr && found->GetState() == ModuleState::kActive)
        {
            mCached = static_cast<T*>(found);
            return mCached;
        }

        return nullptr;
    }

    template<typename T>
    const T* ModuleRef<T>::Get() const
    {
        return const_cast<ModuleRef<T>*>(this)->Get();
    }

    template<typename T>
    T* ModuleRef<T>::operator->()
    {
        T* ptr = Get();
        DIA_ASSERT(ptr != nullptr,
                   "ModuleRef: dereferencing null reference to '%s'",
                   mTargetId.AsChar());
        return ptr;
    }

    template<typename T>
    const T* ModuleRef<T>::operator->() const
    {
        return const_cast<ModuleRef<T>*>(this)->operator->();
    }

    template<typename T>
    ModuleRef<T>::operator bool() const
    {
        return Get() != nullptr;
    }

}} // namespace Dia::ApplicationFlow
