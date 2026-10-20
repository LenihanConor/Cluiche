////////////////////////////////////////////////////////////////////////////////
// Filename: HexFlatTopOddQBuilder.cpp
////////////////////////////////////////////////////////////////////////////////
#include "HexFlatTopOddQBuilder.h"

#include <math.h>

namespace Dia
{
	namespace Graphics
	{
		namespace
		{
			const float kPi = 3.14159265358979323846f;

			void Quantise(const bool present[4], const float raw[4], uint8_t out[4])
			{
				float sum = 0.0f;
				for (int i = 0; i < 4; ++i)
					sum += present[i] ? raw[i] : 0.0f;

				int values[4] = { 0, 0, 0, 0 };
				int total = 0;
				int largest = 0;
				for (int i = 0; i < 4; ++i)
				{
					const float w = present[i] ? raw[i] / sum : 0.0f;
					values[i] = static_cast<int>(w * 255.0f + 0.5f);
					total += values[i];
					if (values[i] > values[largest])
						largest = i;
				}
				values[largest] += 255 - total;
				for (int i = 0; i < 4; ++i)
					out[i] = static_cast<uint8_t>(values[i]);
			}
		}

		Dia::Core::StringCRC HexFlatTopOddQBuilder::Topology() const
		{
			return Dia::Core::StringCRC("hex.flat.oddq");
		}

		bool HexFlatTopOddQBuilder::Build(const CellGridDesc& desc, CellGridGeometry& out) const
		{
			if (!(desc.topology == Topology()) || desc.width == 0 || desc.height == 0 ||
				desc.centresXY == nullptr || desc.neighbours == nullptr)
				return false;

			const uint32_t cells = static_cast<uint32_t>(desc.width) * desc.height;
			for (uint32_t i = 0; i < cells * 6u; ++i)
				if (desc.neighbours[i] >= static_cast<int32_t>(cells) || desc.neighbours[i] < -1)
					return false;

			float cornerX[7], cornerY[7];
			for (int k = 0; k < 7; ++k)
			{
				const float a = kPi / 3.0f * static_cast<float>(k % 6);
				cornerX[k] = cosf(a);
				cornerY[k] = -sinf(a);
			}

			out.Allocate(cells * 18u, cells * 18u);

			VertexCellField* v = out.vertices;
			for (uint32_t c = 0; c < cells; ++c)
			{
				const float cx = desc.centresXY[2u * c];
				const float cy = desc.centresXY[2u * c + 1u];
				const int32_t* nb = desc.neighbours + 6u * c;

				for (uint32_t k = 0; k < 6u; ++k)
				{
					const int32_t refs[4] = { static_cast<int32_t>(c), nb[(k + 5u) % 6u], nb[k], nb[(k + 1u) % 6u] };
					bool present[4];
					for (int i = 0; i < 4; ++i)
						present[i] = refs[i] >= 0;

					const float third = 1.0f / 3.0f;
					const float wCentre[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
					const float wK[4]      = { third, third, third, 0.0f };
					const float wK1[4]     = { third, 0.0f, third, third };

					VertexCellField tri[3];
					tri[0].offset[0] = 0.0f;           tri[0].offset[1] = 0.0f;
					tri[1].offset[0] = cornerX[k];     tri[1].offset[1] = cornerY[k];
					tri[2].offset[0] = cornerX[k + 1]; tri[2].offset[1] = cornerY[k + 1];
					Quantise(present, wCentre, tri[0].weights);
					Quantise(present, wK, tri[1].weights);
					Quantise(present, wK1, tri[2].weights);

					for (int j = 0; j < 3; ++j)
					{
						tri[j].centre[0] = cx;
						tri[j].centre[1] = cy;
						for (int i = 0; i < 4; ++i)
							tri[j].cellIndices[i] = present[i] ? static_cast<float>(refs[i]) : -1.0f;
						*v++ = tri[j];
					}
				}
			}

			for (uint32_t i = 0; i < out.indexCount; ++i)
				out.indices[i] = i;
			return true;
		}
	}
}
