#pragma once

#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/ArrayC.h>

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

		// This is meta data for each level that is stored at any point but does not allocate the level
		class LevelMetaData
		{
		public:
			LevelMetaData();
		
			bool IsLoaded()const { return mIsLoaded; }

		private:
			bool mIsLoaded;

			ILevel* mAssociatedLevel;
		};

		/// This is a registry of all level
		class LevelRegistry
		{
		public:
			LevelRegistry();
			~LevelRegistry();
			
			void AddLevelMetaData(const LevelMetaData& data);

			void CreateLevel(ILevel* level);
			void DeleteLevel(const Dia::Core::StringCRC& uniqueId);
			
			void SetCurrentLevel(ILevel* level);
			ILevel* GetCurrentLevel();
			const ILevel* GetCurrentLevel()const;

			ILevel* FindLevel(const Dia::Core::StringCRC& uniqueId);
			const ILevel* FindLevel(const Dia::Core::StringCRC& uniqueId)const;

		private:
			static const int kMaxLevels = 32;
			typedef Dia::Core::Containers::ArrayC<LevelMetaData, kMaxLevels> LevelMetaDataArray;

			ILevel* mCurrentLevel;

			LevelMetaDataArray mLevelArray;
		};
	}
}
