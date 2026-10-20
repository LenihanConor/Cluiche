////////////////////////////////////////////////////////////////////////////////
// Filename: ICellGridGeometryBuilder.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/CellField/CellGridDesc.h>

#include <memory>
#include <stdint.h>

namespace Dia
{
	namespace Graphics
	{
		///
		/// Per-vertex data for the CellField shader (36 bytes).
		/// The shader places the vertex at centre + offset * cellSize * (1 - inset).
		///
		struct VertexCellField
		{
			float   centre[2];			// cell centre (world)
			float   offset[2];			// unit-hex corner offset from the centre
			float   cellIndices[4];		// [own, N(k-1), N(k), N(k+1)]; -1 = none
			uint8_t weights[4];			// unorm8, sums to exactly 255
		};
		static_assert(sizeof(VertexCellField) == 36, "VertexCellField must be 36 bytes");

		///
		/// CellGridGeometry - CPU vertex/index arrays. Owns its buffers (move-only RAII).
		/// Indices are uint32_t: a 128x128 hex grid has 294,912 vertices, beyond uint16.
		///
		struct CellGridGeometry
		{
			uint32_t         vertexCount = 0;
			uint32_t         indexCount = 0;
			VertexCellField* vertices = nullptr;
			uint32_t*        indices = nullptr;

			void Allocate(uint32_t numVertices, uint32_t numIndices)
			{
				mVertexStorage.reset(new VertexCellField[numVertices]);
				mIndexStorage.reset(new uint32_t[numIndices]);
				vertices = mVertexStorage.get();
				indices = mIndexStorage.get();
				vertexCount = numVertices;
				indexCount = numIndices;
			}

			void Clear()
			{
				mVertexStorage.reset();
				mIndexStorage.reset();
				vertices = nullptr;
				indices = nullptr;
				vertexCount = 0;
				indexCount = 0;
			}

		private:
			std::unique_ptr<VertexCellField[]> mVertexStorage;
			std::unique_ptr<uint32_t[]>        mIndexStorage;
		};

		///
		/// ICellGridGeometryBuilder - builds CellField geometry on the CPU (no GPU).
		///
		class ICellGridGeometryBuilder
		{
		public:
			virtual ~ICellGridGeometryBuilder() = default;
			virtual Dia::Core::StringCRC Topology() const = 0;

			/// Returns false (and leaves 'out' untouched) on unsupported topology or invalid data.
			virtual bool Build(const CellGridDesc& desc, CellGridGeometry& out) const = 0;
		};
	}
}
