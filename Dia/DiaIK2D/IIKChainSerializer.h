#pragma once

#include <DiaSerializer/ISerializer.h>
#include <DiaSerializer/SerializeResult.h>
#include <DiaIK2D/IKChainDef.h>

namespace Dia
{
	namespace IK2D
	{
		class IIKChainSerializer : public Dia::Serializer::ISerializer
		{
		public:
			virtual Dia::Serializer::SerializeResult Load(const char* data, IKChainDef& outDef) const = 0;
			virtual Dia::Serializer::SerializeResult Save(const IKChainDef& def, char* outBuffer, unsigned int bufferSize) const = 0;

			Dia::Serializer::SerializeResult LoadFromFile(const char* path, IKChainDef& outDef) const;
			Dia::Serializer::SerializeResult SaveToFile(const char* path, const IKChainDef& def) const;
		};
	}
}
