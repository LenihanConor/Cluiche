#include "ApplicationFlow/ProcessingUnits/SimProcessingUnit.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimUIProxyModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h"

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaSFML/TextureHandler.h>
#include <DiaUI/IUISystem.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/DiaLog.h>

namespace Cluiche
{
	Dia::Maths::Vector2D dynamicCirclePos(0.0f, 0.0f);
	Dia::Graphics::RGBA dynamicCircleColour = Dia::Graphics::RGBA::Red;

	const Dia::Core::StringCRC SimProcessingUnit::kTypeId("SimProcessingUnit");

	SimProcessingUnit::SimProcessingUnit(const Dia::Core::StringCRC& instanceId, float hz)
		: Dia::Application::ProcessingUnit(instanceId, hz)
		, mThreadBufferRegistered(false)
		, mRunning(nullptr)
		, mSimToRenderFrameStream(nullptr)
		, mCanvas(nullptr)
		, mTextureHandler(nullptr)
	{}

	void SimProcessingUnit::PostPhaseStart(const Dia::Application::StateObject::IStartData* startData)
	{
		DIA_ASSERT(startData != nullptr, "Starting the render PU needs to created with some start data");

		const StartData* simStartData = static_cast<const StartData*>(startData);
		mRunning = simStartData->mRunning;
		mSimToRenderFrameStream = simStartData->mFrameStream;
		mCanvas = simStartData->mCanvas;
		mTextureHandler = simStartData->mTextureHandler;

		GetModule<Cluiche::Sim::UIProxyModule>()->Initialize(simStartData->mMainUIModule, &mRenderFrameBuffer);
		GetModule<Cluiche::Sim::InputFrameStreamModule>()->Initialize(simStartData->mInputToSimFrameStream);
	}

	void SimProcessingUnit::PrePhaseUpdate()
	{
		if (!mThreadBufferRegistered)
		{
			Dia::Logger::Logger::Instance().RegisterThreadBuffer();
			DIA_LOG_INFO("Application", "Sim thread registered for logging");
			mThreadBufferRegistered = true;
		}

		mRenderFrameBuffer.Clear();
	}

	void SimProcessingUnit::PostPhaseUpdate()
	{
		auto* inputModule = GetModule<Cluiche::Sim::InputFrameStreamModule>();
		auto* timeModule = GetModule<Cluiche::Sim::TimeServerModule>();

		inputModule->GetStream()->GarbageCollectAllFramesOlderThan(timeModule->GetTimeServer().GetTime());

		// Draw debug circles and lines
		mRenderFrameBuffer.RequestDraw(Dia::Maths::Vector2D(100.0f, 100.0f), 75.0f, Dia::Graphics::RGBA::White);
		mRenderFrameBuffer.RequestDraw(dynamicCirclePos, 25.0f, dynamicCircleColour);
		mRenderFrameBuffer.RequestDraw(Dia::Maths::Vector2D(100.0f, 100.0f), dynamicCirclePos, Dia::Graphics::RGBA::White);

		// Draw sprites using texture IDs from AssetRuntime (only available after stage load)
		if (mTextureHandler)
		{
			unsigned int redId = mTextureHandler->GetTextureId(Dia::Core::StringCRC("texture.test_red"));
			unsigned int blueId = mTextureHandler->GetTextureId(Dia::Core::StringCRC("texture.test_blue"));
			unsigned int greenId = mTextureHandler->GetTextureId(Dia::Core::StringCRC("texture.test_green"));

			if (redId != 0)
			{
				Dia::Graphics::SpriteDrawCommand redSprite(redId, Dia::Maths::Vector2D(200.0f, 200.0f));
				mRenderFrameBuffer.RequestDrawSprite(redSprite);

				Dia::Graphics::SpriteDrawCommand transparentSprite(redId, Dia::Maths::Vector2D(200.0f, 300.0f));
				transparentSprite.tint = Dia::Graphics::RGBA(255, 255, 255, 128);
				mRenderFrameBuffer.RequestDrawSprite(transparentSprite);
			}

			if (blueId != 0)
			{
				Dia::Graphics::SpriteDrawCommand blueSprite(blueId, Dia::Maths::Vector2D(300.0f, 200.0f));
				blueSprite.scale = Dia::Maths::Vector2D(1.5f, 1.5f);
				mRenderFrameBuffer.RequestDrawSprite(blueSprite);

				Dia::Graphics::SpriteDrawCommand dynamicSprite(blueId, dynamicCirclePos);
				dynamicSprite.scale = Dia::Maths::Vector2D(0.5f, 0.5f);
				mRenderFrameBuffer.RequestDrawSprite(dynamicSprite);
			}

			if (greenId != 0)
			{
				Dia::Graphics::SpriteDrawCommand greenSprite(greenId, Dia::Maths::Vector2D(400.0f, 200.0f));
				greenSprite.rotation = 45.0f;
				mRenderFrameBuffer.RequestDrawSprite(greenSprite);
			}
		}

		mSimToRenderFrameStream->InsertCopyOfDataToStream(mRenderFrameBuffer, timeModule->GetTimeServer().GetTime());

		timeModule->Tick();
	}

	bool SimProcessingUnit::FlaggedToStopUpdating()const
	{
		bool isFlaggedToStop = !(*mRunning);
		if (isFlaggedToStop && mThreadBufferRegistered)
		{
			Dia::Logger::Logger::Instance().UnregisterThreadBuffer();
			const_cast<SimProcessingUnit*>(this)->mThreadBufferRegistered = false;
		}
		return isFlaggedToStop;
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _SimProcessingUnit = Cluiche::SimProcessingUnit; }
DIA_REGISTER_PROCESSING_UNIT(_SimProcessingUnit) {
	float hz = config.get("frequency_hz", 30.0f).asFloat();
	return new Cluiche::SimProcessingUnit(instanceId, hz);
}