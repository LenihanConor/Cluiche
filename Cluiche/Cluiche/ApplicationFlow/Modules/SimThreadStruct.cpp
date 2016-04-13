#include "SimThreadStruct.h"

#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaInput/InputSourceManager.h>

#include <iostream>

Dia::Maths::Vector2D dynamicCirclePos(0.0f, 0.0f);
Dia::Graphics::RGBA dynamicCircleColour = Dia::Graphics::RGBA::Red;

SimThreadStruct::SimThreadStruct()
	: mRunning(nullptr)
	, mTimeServer(30.0f, Dia::Core::TimeAbsolute::Zero()) 
	, mUISystem(nullptr)
	, mInputToSimFrameStream(nullptr)
	, mSimToRenderFrameStream(nullptr)
{}

void SimThreadStruct::Initialize(bool* running,
									Dia::UI::IUISystem* uiSystem,
									Dia::Core::FrameStream<Dia::Input::EventData>* inputToSimFrameStream,
									Dia::Core::FrameStream<Dia::Graphics::FrameData>* simToRenderFrameStream)
{
	mRunning = running;
	mUISystem = uiSystem;
	mInputToSimFrameStream = inputToSimFrameStream;
	mSimToRenderFrameStream = simToRenderFrameStream;
	mThreadLimiter.Initialize(1.0f / mTimeServer.GetStep().AsFloatInSeconds());
}

void SimThreadStruct::operator()()
{
	Run();
}

void SimThreadStruct::Run()
{
	// This is where the simulation (Model) would go...

	// This below is just a test
	while (*mRunning == true)
	{
		mThreadLimiter.Start();

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
		Dia::UI::UIDataBuffer uiBuffer;
		mUISystem->FetchUIDataBuffer(uiBuffer);
		renderFrameBuffer.RequestDrawUI(uiBuffer);

		mSimToRenderFrameStream->InsertCopyOfDataToStream(renderFrameBuffer, mTimeServer.GetTime());

		mTimeServer.Tick();

		mThreadLimiter.Stop();
//		std::cout << "SIM: Wait " << mThreadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
		mThreadLimiter.SleepThread();
	}
}


