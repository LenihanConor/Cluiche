#pragma once

#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche
{
	/// This is a registry of all level
	class LevelRegistry
	{
	public:
		struct Data
		{
			Data(){}
			Data(const Dia::Core::StringCRC& entryPhaseUniqueId) : mEntryPhaseUniqueId(entryPhaseUniqueId){}

			Dia::Core::StringCRC mEntryPhaseUniqueId;
		};

		LevelRegistry();
		~LevelRegistry();
	
		void LevelRegistry::Register(const Dia::Core::StringCRC& name, const LevelRegistry::Data& data);
		const Dia::Core::StringCRC& FetchEntryPhase(Dia::Core::StringCRC& levelName)const;

	private:
		typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Data, Dia::Core::StringCRCHashFunctor> LevelTable;
		
		// Essentially this is a hash of name to struct
		LevelTable mTable; 
	};
}
