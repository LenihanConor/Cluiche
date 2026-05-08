#pragma once

#include <DiaSerializer/ISerializer.h>
#include <DiaSerializer/SerializeResult.h>
#include <DiaGame/DiaGameManifest.h>

namespace Dia
{
	namespace Game
	{
		class JsonDiaStageSerializer : public Dia::Serializer::ISerializer
		{
		public:
			const char* GetVersion() const override;

			Dia::Serializer::SerializeResult Load(const char* data, DiaStageManifest& outManifest) const;
			Dia::Serializer::SerializeResult Save(const DiaStageManifest& manifest, char* outBuffer, unsigned int bufferSize) const;

			Dia::Serializer::SerializeResult LoadFromFile(const char* path, DiaStageManifest& outManifest) const;
			Dia::Serializer::SerializeResult SaveToFile(const char* path, const DiaStageManifest& manifest) const;
		};
	}
}
