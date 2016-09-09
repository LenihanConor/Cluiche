////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationStateObject.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _APPLICATIONSTATEOBJECT_H_
#define _APPLICATIONSTATEOBJECT_H_

#include <DiaCore/Core/EnumClass.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/StringCRC.h>

#include <mutex>

namespace Dia
{
	namespace Application
	{	
		class StateObject;
		class Module;
		class Phase;
		class ProcessingUnit;

		////////////////////////////////////////////////////////////////////////////////
		// Class name: IBuildDependencyData
		////////////////////////////////////////////////////////////////////////////////
		class IBuildDependencyData
		{
		public:
			virtual Module* GetModule(const Dia::Core::StringCRC& crc) = 0;
			virtual const Module* GetModule(const Dia::Core::StringCRC& crc)const = 0;

			virtual Phase* GetPhase(const Dia::Core::StringCRC& crc) = 0;
			virtual const Phase* GetPhase(const Dia::Core::StringCRC& crc)const = 0;

			virtual ProcessingUnit* GetProcessingUnit(const Dia::Core::StringCRC& crc) = 0;
			virtual const ProcessingUnit* GetProcessingUnit(const Dia::Core::StringCRC& crc)const = 0;
		};
		
		////////////////////////////////////////////////////////////////////////////////
		// Class name: StateObject
		////////////////////////////////////////////////////////////////////////////////
		class StateObject
		{
		public:
			////////////////////////////////////////////////////////////////////////////////
			// Enum name: StateEnum, Has basic start/stop. Can go to Idle or Updating dependant 
			//				on whether there is anything to be updated
			////////////////////////////////////////////////////////////////////////////////
			CLASSEDENUM (StateEnum,\
				CE_ITEMVAL(kConstructed, 0)\
				CE_ITEM(kFlaggedToStart)\
				CE_ITEM(kRunning)
				CE_ITEM(kFlaggedToStop)\
				CE_ITEM(kNotRunning)\
				, kNotRunning \
			);

			////////////////////////////////////////////////////////////////////////////////
			// Enum name: OpertionResponse, When calling Start/Stop should we wait or just 
			//				continue on immediately
			////////////////////////////////////////////////////////////////////////////////
			CLASSEDENUM(OpertionResponse, \
				CE_ITEMVAL(kImmediate, 0)\
				CE_ITEM(kAsync)\
				, kImmediate \
				);

			class IStartData
			{
			public:
				IStartData() {};
				virtual ~IStartData() {};
			};

			StateObject(const Dia::Core::StringCRC& uniqueId);

			void BuildDependancies(IBuildDependencyData* buildDependencies);

			OpertionResponse Start(const IStartData* startData = nullptr);
			void NotifyReadyToStartAsync();

			void Update();

			void Stop();							// All modules turning off
			
			const Dia::Core::StringCRC& GetUniqueId()const{ return mUniqueId; }
			virtual const char* GetStateObjectType()const = 0;

			StateEnum GetState()const { return mState; }

			bool HasStarted()const { return (mState == StateEnum::kRunning); }

		protected:
			virtual void DoBuildDependancies(IBuildDependencyData* buildDependencies) = 0;

			virtual OpertionResponse DoStart(const IStartData* startData) = 0;

			virtual void DoUpdate() = 0;

			virtual void DoStop() = 0;

		private:
			StateObject(){};
			
			Dia::Core::StringCRC mUniqueId;
			StateEnum mState;

			std::mutex mStateMutex;
		};
	}
}

#endif