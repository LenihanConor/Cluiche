#pragma once

#include <DiaIK2D/IIKChainSerializer.h>

namespace Dia
{
	namespace IK2D
	{
		class JsonIKChainSerializer : public IIKChainSerializer
		{
		public:
			const char* GetVersion() const override;

			Dia::Serializer::SerializeResult Load(const char* data, IKChainDef& outDef) const override;
			Dia::Serializer::SerializeResult Save(const IKChainDef& def, char* outBuffer, unsigned int bufferSize) const override;
		};
	}
}
