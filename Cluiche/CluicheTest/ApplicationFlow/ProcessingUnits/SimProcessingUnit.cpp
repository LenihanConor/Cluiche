#include "ApplicationFlow/ProcessingUnits/SimProcessingUnit.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaSFML/RenderWindow.h>
#include <DiaUI/IUISystem.h>

namespace Cluiche
{
	Dia::Maths::Vector2D dynamicCirclePos(0.0f, 0.0f);
	Dia::Graphics::RGBA dynamicCircleColour = Dia::Graphics::RGBA::Red;

	const Dia::Core::StringCRC SimProcessingUnit::kUniqueId("SimProcessingUnit");

	SimProcessingUnit::SimProcessingUnit()
		: Dia::Application::ProcessingUnit(kUniqueId, 30.0f)
		, mRunning(nullptr)
		, mSimToRenderFrameStream(nullptr)
		, mCanvas(nullptr)
		, mTestRedTexture(0)
		, mTestBlueTexture(0)
		, mTestGreenTexture(0)
		, mBootPhase(this)
		, mBootStrapPhase(this)
		, mSimTimeServerModule(this, 30.0f, Dia::Core::TimeAbsolute::Zero())
		, mSimUIProxyModule(this, &mRenderFrameBuffer)
		, mSimInputFrameStreamModule(this)
	{
		// Setup Phase Transitions
		SetInitialPhase(&mBootPhase);
		AddPhaseTransiton(&mBootPhase, &mBootStrapPhase);

		// We call this from here to build all dependancies. We dont need to do this here, intead 
		// we could of done it in the code below if there was dynamic adding of modules or phases
		// but for this cas case this is a nicer cleaner solution.
		Initialize();
	}

	void SimProcessingUnit::PostPhaseStart(const Dia::Application::StateObject::IStartData* startData)
	{
		DIA_ASSERT(startData != nullptr, "Starting the render PU needs to created with some start data");

		const StartData* simStartData = static_cast<const StartData*>(startData);
		mRunning = simStartData->mRunning;
		mSimToRenderFrameStream = simStartData->mFrameStream;
		mCanvas = simStartData->mCanvas;

		GetModule<Cluiche::Sim::UIProxyModule>()->Initialize(simStartData->mMainUIModule);
		GetModule<Cluiche::Sim::InputFrameStreamModule>()->Initialize(simStartData->mInputToSimFrameStream);

		// Load test textures
		// Note: This is thread-safe as TextureManager has internal mutex protection
		if (mCanvas)
		{
			// Cast to access SFML-specific LoadTexture method
			Dia::SFML::RenderWindow* renderWindow = dynamic_cast<Dia::SFML::RenderWindow*>(mCanvas);
			if (renderWindow)
			{
				mTestRedTexture = renderWindow->LoadTexture("Assets/Textures/test_red.png");
				mTestBlueTexture = renderWindow->LoadTexture("Assets/Textures/test_blue.png");
				mTestGreenTexture = renderWindow->LoadTexture("Assets/Textures/test_green.png");
			}
		}
	}

	void SimProcessingUnit::PrePhaseUpdate()
	{
		mRenderFrameBuffer.Clear();
	}

	void SimProcessingUnit::PostPhaseUpdate()
	{
		// TODO MOVE TO THE PHASE
		// All going to Sim thread
		mSimInputFrameStreamModule.GetStream()->GarbageCollectAllFramesOlderThan(mSimTimeServerModule.GetTimeServer().GetTime());

		// Draw debug circles and lines
		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(Dia::Maths::Vector2D(100.0f, 100.0f), 75.0f));
		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(dynamicCirclePos, 25.0f, dynamicCircleColour));
		mRenderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataLine2D(Dia::Maths::Vector2D(100.0f, 100.0f), dynamicCirclePos));

		// TEST: Draw sprites with various transformations
		if (mTestRedTexture != 0)
		{
			// Static red sprite at (200, 200)
			Dia::Graphics::SpriteDrawCommand redSprite(mTestRedTexture, Dia::Maths::Vector2D(200.0f, 200.0f));
			mRenderFrameBuffer.RequestDrawSprite(redSprite);

			// Blue sprite at (300, 200) with 1.5x scale
			Dia::Graphics::SpriteDrawCommand blueSprite(mTestBlueTexture, Dia::Maths::Vector2D(300.0f, 200.0f));
			blueSprite.scale = Dia::Maths::Vector2D(1.5f, 1.5f);
			mRenderFrameBuffer.RequestDrawSprite(blueSprite);

			// Green sprite at (400, 200) with 45 degree rotation
			Dia::Graphics::SpriteDrawCommand greenSprite(mTestGreenTexture, Dia::Maths::Vector2D(400.0f, 200.0f));
			greenSprite.rotation = 45.0f;
			mRenderFrameBuffer.RequestDrawSprite(greenSprite);

			// Red sprite at (200, 300) with 50% transparency
			Dia::Graphics::SpriteDrawCommand transparentSprite(mTestRedTexture, Dia::Maths::Vector2D(200.0f, 300.0f));
			transparentSprite.tint = Dia::Graphics::RGBA(255, 255, 255, 128);
			mRenderFrameBuffer.RequestDrawSprite(transparentSprite);

			// Blue sprite following the dynamic circle position
			Dia::Graphics::SpriteDrawCommand dynamicSprite(mTestBlueTexture, dynamicCirclePos);
			dynamicSprite.scale = Dia::Maths::Vector2D(0.5f, 0.5f);
			mRenderFrameBuffer.RequestDrawSprite(dynamicSprite);
		}

		mSimToRenderFrameStream->InsertCopyOfDataToStream(mRenderFrameBuffer, mSimTimeServerModule.GetTimeServer().GetTime());

		mSimTimeServerModule.Tick();
	}

	bool SimProcessingUnit::FlaggedToStopUpdating()const
	{
		bool isFlaggeToStop = !(*mRunning);
		return isFlaggeToStop;
	}
}