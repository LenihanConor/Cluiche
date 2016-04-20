#include "ApplicationFlow/ProcessingUnits/SimProcessingUnit.h"

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaUI/IUISystem.h>

namespace Cluiche
{
	Dia::Maths::Vector2D dynamicCirclePos(0.0f, 0.0f);
	Dia::Graphics::RGBA dynamicCircleColour = Dia::Graphics::RGBA::Red;

	const Dia::Core::StringCRC SimProcessingUnit::kUniqueId("SimProcessingUnit");

	SimProcessingUnit::SimProcessingUnit()
		: Dia::Application::ProcessingUnit(kUniqueId, 30.0f)
		, mTimeServer(30.0f, Dia::Core::TimeAbsolute::Zero())
		, mRunning(nullptr)
		, mUISystem(nullptr)
		, mInputToSimFrameStream(nullptr)
		, mSimToRenderFrameStream(nullptr)
		, mBootPhase(this)
		, mBootStrapPhase(this)
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
		mUISystem = simStartData->mUISystem;
		mInputToSimFrameStream = simStartData->mInputToSimFrameStream;
	}

	void SimProcessingUnit::PostPhaseUpdate()
	{
		// All going to Sim thread

		Dia::Core::Containers::DynamicArrayC<const Dia::Input::EventData*, 32> inputEventPtrBuffer;
		mInputToSimFrameStream->FetchAllDataUpToTime(mTimeServer.GetTime(), inputEventPtrBuffer);

		for (unsigned int i = 0; i < inputEventPtrBuffer.Size(); i++)
		{
			const Dia::Input::EventData* pEventData = inputEventPtrBuffer[i];

			// This is where an application will handle any user input
			for (unsigned int j = 0; j < pEventData->Size(); j++)
			{
				Dia::Input::Event event = (*pEventData)[j];
				switch (event.type)
				{
				case Dia::Input::Event::EType::kConsoleGamepadAnalogStickMove:
					dynamicCirclePos += Dia::Maths::Vector2D(event.consoleGamepadMoveEvent.x, event.consoleGamepadMoveEvent.y);
					break;
				case Dia::Input::Event::EType::kConsoleGamepadButtonPressed:
					switch (event.consoleGamepadButtonEvent.button)
					{
					case Dia::Input::ConsoleGamepad::EButtonID::A:
						dynamicCircleColour = Dia::Graphics::RGBA::Green;
						break;
					case Dia::Input::ConsoleGamepad::EButtonID::B:
						dynamicCircleColour = Dia::Graphics::RGBA::Red;
						break;
					case Dia::Input::ConsoleGamepad::EButtonID::X:
						dynamicCircleColour = Dia::Graphics::RGBA::Blue;
						break;
					case Dia::Input::ConsoleGamepad::EButtonID::Y:
						dynamicCircleColour = Dia::Graphics::RGBA::Yellow;
						break;
					}

					break;
				case Dia::Input::Event::EType::kMouseMoved:
				{
					//			mUISystem.InjectMouseMove(event.mouseMove.x, event.mouseMove.y);
				}
				break;
				case Dia::Input::Event::EType::kMouseButtonPressed:
				{
					//			mUISystem.InjectMouseDown(event.mouseButton.AsMouseButton());
				}
				break;
				default:
					break;
				}
			}
		}

		mInputToSimFrameStream->GarbageCollectAllFramesOlderThan(mTimeServer.GetTime());

		Dia::Graphics::FrameData renderFrameBuffer;

		renderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(Dia::Maths::Vector2D(100.0f, 100.0f), 75.0f));
		renderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataCircle2D(dynamicCirclePos, 25.0f, dynamicCircleColour));
		renderFrameBuffer.RequestDraw(Dia::Graphics::DebugFrameDataLine2D(Dia::Maths::Vector2D(100.0f, 100.0f), dynamicCirclePos));

		// Update 
		if (mUISystem->IsPageLoaded())
		{
			Dia::UI::UIDataBuffer uiBuffer;
			mUISystem->FetchUIDataBuffer(uiBuffer);
			renderFrameBuffer.RequestDrawUI(uiBuffer);
		}

		mSimToRenderFrameStream->InsertCopyOfDataToStream(renderFrameBuffer, mTimeServer.GetTime());

		mTimeServer.Tick();
	}

	bool SimProcessingUnit::FlaggedToStopUpdating()const
	{
		bool isFlaggeToStop = !(*mRunning);
		return isFlaggeToStop;
	}
}