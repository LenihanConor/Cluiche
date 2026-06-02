#include <DiaCore/Metadata/DescriptionRegistry.h>
#include <vector>

namespace Dia::Core::Metadata {

namespace {
	std::vector<DescriptionRegistry::Entry>& GetRegistry() {
		static std::vector<DescriptionRegistry::Entry> registry;
		return registry;
	}
}

void DescriptionRegistry::Register(StringCRC id, const char* description) {
	GetRegistry().push_back({id, description});
}

const char* DescriptionRegistry::Get(StringCRC id) {
	const auto& registry = GetRegistry();
	for (const auto& entry : registry) {
		if (entry.id == id) {
			return entry.description;
		}
	}
	return nullptr;
}

}
