#include <DiaCore/Metadata/DescriptionRegistry.h>

namespace Dia::Core::Metadata {

DynamicArrayC<DescriptionRegistry::Entry>& DescriptionRegistry::GetRegistry() {
	static DynamicArrayC<Entry> registry;
	return registry;
}

void DescriptionRegistry::Register(StringCRC id, const char* description) {
	auto& registry = GetRegistry();
	registry.PushBack({id, description});
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

const DynamicArrayC<DescriptionRegistry::Entry>& DescriptionRegistry::GetAll() {
	return GetRegistry();
}

}
