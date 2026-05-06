#pragma once

#include <DiaCore/Strings/String256.h>

namespace Dia
{
	namespace Application
	{
		struct TypedImport
		{
			enum class ImportType
			{
				kManifest,
				kStage
			};

			Dia::Core::Containers::String256 path;
			ImportType type;

			TypedImport() : type(ImportType::kManifest) {}
			TypedImport(const char* inPath, ImportType inType) : path(inPath), type(inType) {}
		};
	}
}
