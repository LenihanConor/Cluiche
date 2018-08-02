#pragma once

#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/CRC/StringCRC.h>

//TODO MOVE THESE OUT
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaApplication/ApplicationPhase.h>

namespace Cluiche
{
	namespace Kernel
	{
		class ApplicationPointerBundle
		{
		public:
			typedef Dia::Core::Containers::DynamicArrayC<Dia::Application::Phase*, 16> PhasePointerList;
			typedef Dia::Core::Containers::DynamicArrayC<Dia::Application::Module*, 64> ModulePointerList;

			void Add(Dia::Application::Phase* phase)
			{
				mPhases.Add(phase);
			}
			void Add(Dia::Application::Module* module)
			{
				mModules.Add(module);
			}

			const PhasePointerList& GetPhases()const { return mPhases; }
			const ModulePointerList& GetModules()const { return mModules; }

		private:
			PhasePointerList mPhases;
			ModulePointerList mModules;
		};

		class ILevel
		{
		public:
			virtual const Dia::Core::StringCRC& GetUniqueId()const = 0;
			virtual const Dia::Core::StringCRC& GetEntryPhaseUniqueId()const = 0;
			virtual const Dia::Core::StringCRC& GetExitPhaseUniqueId()const = 0;

			void AddToMainThreadBundle(Dia::Application::Phase* phase)
			{
				mMainApplicationBundle.Add(phase);
			}
			void AddToMainThreadBundle(Dia::Application::Module* module)
			{
				mMainApplicationBundle.Add(module);
			}

			const ApplicationPointerBundle& GetMainThreadApplicationBundle()const {return mMainApplicationBundle;}

		private:
			ApplicationPointerBundle mMainApplicationBundle;
		};

		/// This is a registry of all level
		class LevelRegistry
		{
		public:
			LevelRegistry();
			~LevelRegistry();

			void SetCurrentLevel(ILevel* level);
			void DeleteLevel();
			
			ILevel* GetCurrentLevel();
			const ILevel* GetCurrentLevel()const;

		private:
			ILevel* mCurrentLevel;
		};
	}
}
