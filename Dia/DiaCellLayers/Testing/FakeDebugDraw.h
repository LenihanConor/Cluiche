#pragma once

// Test helper (Testing/ convention): IDebugDraw that records CellField draw requests and exposes a settable mouse pixel.

#include <DiaCore/DebugDraw/IDebugDraw.h>

namespace Dia::CellLayers::Testing
{
	struct FakeDebugDraw : public Dia::Core::IDebugDraw
	{
		uint32_t cellFieldRequests = 0;
		Dia::Core::CellFieldDrawRequest lastRequest;
		Dia::Maths::Vector2D mouse;

		void RequestDraw(const Dia::Maths::Vector2D&, float, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
		void RequestDrawPoint(const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
		void RequestDrawRect(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDrawArc(const Dia::Maths::Vector2D&, float, float, float, Dia::Core::RGBA) override {}
		void RequestDrawRay(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, float, Dia::Core::RGBA) override {}
		void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDrawText(const Dia::Maths::Vector2D&, const char*, float, Dia::Core::RGBA) override {}
		void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
		void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
		void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
		void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
		void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, float, Dia::Core::RGBA) override {}
		void RequestDrawCellField(const Dia::Core::CellFieldDrawRequest& r) override { ++cellFieldRequests; lastRequest = r; }
		uint32_t DroppedCount() const override { return 0; }
		const Dia::Maths::Vector2D& GetMousePixel() const override { return mouse; }
	};
}
