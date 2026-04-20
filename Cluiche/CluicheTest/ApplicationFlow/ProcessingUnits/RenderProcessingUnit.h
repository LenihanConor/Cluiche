#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>

#include <DiaCore/Frame/FrameStream.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace Dia { namespace Graphics { class ICanvas; } }

namespace Cluiche
{
	class RenderProcessingUnit : public Dia::Application::ProcessingUnit
	{
	public:
		class StartData: public Dia::Application::ProcessingUnit::IStartData
		{
		public:
			StartData() : mRunning(nullptr), mFrameStream(nullptr), mCanvas(nullptr) {}

			const bool* mRunning;
			Dia::Core::FrameStream<Dia::Graphics::FrameData>* mFrameStream;
			Dia::Graphics::ICanvas* mCanvas;
		};

		static const Dia::Core::StringCRC kTypeId;

		RenderProcessingUnit(const Dia::Core::StringCRC& instanceId, float hz);

	private:
		virtual void PostPhaseStart(const IStartData* startData) override final;
		virtual void PostPhaseUpdate() override final;
		virtual bool FlaggedToStopUpdating()const override final;

		const bool* mRunning;
		Dia::Core::FrameStream<Dia::Graphics::FrameData>* mFrameStream;
		Dia::Graphics::ICanvas* mpCanvas;
	};
}
