#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Core::Metadata {

class DescriptionRegistry {
public:
	struct Entry {
		StringCRC id;
		const char* description;
	};

	static void Register(StringCRC id, const char* description);
	static const char* Get(StringCRC id);

private:
	DescriptionRegistry() = delete;
};

}
