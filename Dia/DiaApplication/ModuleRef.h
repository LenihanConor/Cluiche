////////////////////////////////////////////////////////////////////////////////
// Filename: ModuleRef.h
//
// Lifecycle-safe handle for referencing modules within the same ProcessingUnit.
// Returns the target only when it is in kRunning state.
//
// Usage:
//   // Declare as module member:
//   ModuleRef<EditorViewModule> mViewRef;
//
//   // Initialize in constructor:
//   MyModule(PU* pu) : Module(pu, ...), mViewRef(this) {}
//
//   // Access in DoUpdate:
//   if (auto* view = mViewRef.Get()) { view->DoSomething(); }
//
////////////////////////////////////////////////////////////////////////////////
#ifndef _MODULEREF_H_
#define _MODULEREF_H_

#include <DiaApplication/ApplicationModule.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Application
	{
		template<class T>
		class ModuleRef
		{
		public:
			explicit ModuleRef(Module* owner);

			T* Get();
			const T* Get() const;

			bool IsRegistered() const;

		private:
			Module* mOwner;
			mutable T* mCached;
		};

		template<class T>
		ModuleRef<T>::ModuleRef(Module* owner)
			: mOwner(owner)
			, mCached(nullptr)
		{
			DIA_ASSERT(mOwner != nullptr, "ModuleRef: owner module cannot be null");
		}

		template<class T>
		T* ModuleRef<T>::Get()
		{
			if (mCached != nullptr && mCached->HasStarted())
				return mCached;

			ProcessingUnit* pu = mOwner->GetAssociatedProcessingUnit();
			Module* found = pu->FindModule(T::kTypeId);

			if (found != nullptr && found->HasStarted())
			{
				mCached = static_cast<T*>(found);
				return mCached;
			}

			mCached = nullptr;
			return nullptr;
		}

		template<class T>
		const T* ModuleRef<T>::Get() const
		{
			if (mCached != nullptr && mCached->HasStarted())
				return mCached;

			const ProcessingUnit* pu = mOwner->GetAssociatedProcessingUnit();
			const Module* found = pu->FindModule(T::kTypeId);

			if (found != nullptr && found->HasStarted())
			{
				mCached = static_cast<T*>(const_cast<Module*>(found));
				return mCached;
			}

			mCached = nullptr;
			return nullptr;
		}

		template<class T>
		bool ModuleRef<T>::IsRegistered() const
		{
			const ProcessingUnit* pu = mOwner->GetAssociatedProcessingUnit();
			return pu->ContainsModule(T::kTypeId);
		}
	}
}

#endif
