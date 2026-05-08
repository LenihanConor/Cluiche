#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaEditor/MVC/EditorModel.h>

namespace Cluiche
{
	namespace Editor
	{
		class EditorModelModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorModelModule(Dia::Application::ProcessingUnit* pu);

			Dia::Editor::EditorModel& GetModel() { return mModel; }
			const Dia::Editor::EditorModel& GetModel() const { return mModel; }

		protected:
			void DoStop() override;

		private:
			Dia::Editor::EditorModel mModel;
		};
	}
}
