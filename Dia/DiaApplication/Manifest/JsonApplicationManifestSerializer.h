#pragma once

#include <DiaSerializer/ISerializer.h>
#include <DiaSerializer/SerializeResult.h>
#include <DiaApplication/Manifest/ApplicationManifest.h>

namespace Dia
{
	namespace Application
	{
		// Serializes ApplicationManifest to/from JSON using DiaSerializer conventions.
		// Load delegates to ApplicationManifestLoader internally; Save produces a
		// round-trippable JSON representation of the manifest structure.
		class JsonApplicationManifestSerializer : public Dia::Serializer::ISerializer
		{
		public:
			const char* GetVersion() const override;

			Dia::Serializer::SerializeResult Load(const char* data, ApplicationManifest& outManifest) const;
			Dia::Serializer::SerializeResult Save(const ApplicationManifest& manifest, char* outBuffer, unsigned int bufferSize) const;

			Dia::Serializer::SerializeResult LoadFromFile(const char* path, ApplicationManifest& outManifest) const;
			Dia::Serializer::SerializeResult LoadFromFile(const char* path, ApplicationManifest& outManifest, char* buffer, unsigned int bufferSize) const;
			Dia::Serializer::SerializeResult SaveToFile(const char* path, const ApplicationManifest& manifest) const;
		};
	}
}
