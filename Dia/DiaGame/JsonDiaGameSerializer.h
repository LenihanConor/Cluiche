#pragma once

#include <DiaSerializer/ISerializer.h>
#include <DiaSerializer/SerializeResult.h>
#include <DiaGame/DiaGameManifest.h>

namespace Dia
{
	namespace Game
	{
		class JsonDiaGameSerializer : public Dia::Serializer::ISerializer
		{
		public:
			const char* GetVersion() const override;

			Dia::Serializer::SerializeResult Load(const char* data, DiaGameManifest& outManifest) const;
			Dia::Serializer::SerializeResult Save(const DiaGameManifest& manifest, char* outBuffer, unsigned int bufferSize) const;

			Dia::Serializer::SerializeResult LoadFromFile(const char* path, DiaGameManifest& outManifest) const;
			Dia::Serializer::SerializeResult SaveToFile(const char* path, const DiaGameManifest& manifest) const;
		};
	}
}
