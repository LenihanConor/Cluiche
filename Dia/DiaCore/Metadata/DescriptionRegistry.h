#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Core::Metadata {

class DescriptionRegistry {
public:
	struct Entry {
		StringCRC id;
		const char* description;
	};

	static void Register(StringCRC id, const char* description);
	static const char* Get(StringCRC id);
	static const DynamicArrayC<Entry>& GetAll();

private:
	static DynamicArrayC<Entry>& GetRegistry();
	DescriptionRegistry() = delete;
};

}
