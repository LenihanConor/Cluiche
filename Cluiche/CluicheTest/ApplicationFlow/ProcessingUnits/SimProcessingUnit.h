#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>

#include <DiaCore/Frame/FrameStream.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaInput/EventData.h>

namespace Dia { namespace Graphics { class ICanvas; } }
namespace Dia { namespace SFML { class TextureHandler; } }
namespace Cluiche { namespace Main { class UIModule; } }

namespace Cluiche
{
	class SimProcessingUnit : public Dia::Application::ProcessingUnit
	{
	public:
		class StartData : public Dia::Application::ProcessingUnit::IStartData
		{
		public:
			StartData() : mRunning(nullptr), mMainUIModule(nullptr), mInputToSimFrameStream(nullptr), mFrameStream(nullptr), mCanvas(nullptr), mTextureHandler(nullptr) {}

			const bool* mRunning;
			Cluiche::Main::UIModule* mMainUIModule;
			Dia::Core::FrameStream<Dia::Input::EventData>* mInputToSimFrameStream;
			Dia::Core::FrameStream<Dia::Graphics::FrameData>* mFrameStream;
			Dia::Graphics::ICanvas* mCanvas;
			Dia::SFML::TextureHandler* mTextureHandler;
		};

		static const Dia::Core::StringCRC kTypeId;

		SimProcessingUnit(const Dia::Core::StringCRC& instanceId, float hz);

		Dia::Graphics::FrameData& GetRenderFrameBuffer() { return mRenderFrameBuffer; }
		Dia::SFML::TextureHandler* GetTextureHandler() const { return mTextureHandler; }

	private:
		virtual void PostPhaseStart(const IStartData* startData) override final;
		virtual void PrePhaseUpdate() override final;
		virtual void PostPhaseUpdate() override final;
		virtual bool FlaggedToStopUpdating()const override final;

		Dia::Graphics::FrameData mRenderFrameBuffer;

		bool mThreadBufferRegistered;
		const bool* mRunning;
		Dia::Core::FrameStream<Dia::Graphics::FrameData>* mSimToRenderFrameStream;
		Dia::Graphics::ICanvas* mCanvas;
		Dia::SFML::TextureHandler* mTextureHandler;
	};
}
