#pragma once

#include <DiaEditor/Project/ProjectContext.h>

namespace Dia
{
	namespace Editor
	{
		class IEditorContext
		{
		public:
			virtual ~IEditorContext() = default;

			virtual bool LoadDiagameProject(const char* diagamePath) = 0;
			virtual void ClearDiagameProject() = 0;
			virtual const ProjectContext& GetDiagameProject() const = 0;
			virtual void OnDiagameProjectChanged(ProjectChangedCallback callback, void* userData) = 0;

			virtual unsigned int GetRecentProjectCount() const = 0;
			virtual const char* GetRecentProject(unsigned int index) const = 0;
		};
	}
}
