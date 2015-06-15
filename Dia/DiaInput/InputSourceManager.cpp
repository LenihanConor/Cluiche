////////////////////////////////////////////////////////////////////////////////
// Filename: DummyClass
////////////////////////////////////////////////////////////////////////////////
#include "DiaInput/InputSourceManager.h"

namespace Dia
{
	namespace Input
	{
		InputSourceManager::InputSourceManager()
		{}

		void InputSourceManager::AddInputSource(IInputSource* inputSource)
		{
			DIA_ASSERT(inputSource, "Cannot be null inputsource");

			mInputSourceList.Add(inputSource);
		}
		void InputSourceManager::RemoveInputSource(IInputSource* inputSource)
		{
			DIA_ASSERT(inputSource, "Cannot be null inputsource");

		//	FindIndex(inputSource->GetUniqueID())
		}

		void InputSourceManager::StartFrame()
		{
			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				mInputSourceList[i]->StartFrame();
			}
		}

		void InputSourceManager::Update(EventStream& outStream)
		{
			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				mInputSourceList[i]->Poll(outStream);
			}
		}

		void InputSourceManager::EndFrame()
		{
			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				mInputSourceList[i]->EndFrame();
			}
		}
	}
}