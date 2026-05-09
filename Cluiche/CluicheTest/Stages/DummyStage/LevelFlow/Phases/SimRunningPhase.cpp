#include "LevelFlow/Phases/SimRunningPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaSFML/TextureHandler.h>

#include "CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimUIProxyModule.h"
#include "ApplicationFlow/ProcessingUnits/SimProcessingUnit.h"

namespace Cluiche
{
	namespace DummyStage
	{
		const Dia::Core::StringCRC SimRunningPhase::kTypeId("DummyStage::SimRunningPhase");

		SimRunningPhase::SimRunningPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Phase(associatedProcessingUnit, kTypeId)
		{}

		void SimRunningPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(Sim::TimeServerModule::kTypeId));
			AddModule(buildDependencies->GetModule(Sim::InputFrameStreamModule::kTypeId));
			AddModule(buildDependencies->GetModule(Sim::UIProxyModule::kTypeId));
		}

		void SimRunningPhase::AfterModulesUpdate()
		{
			Cluiche::SimProcessingUnit* simPU = static_cast<Cluiche::SimProcessingUnit*>(GetAssociatedProcessingUnit());
			Dia::SFML::TextureHandler* textureHandler = simPU->GetTextureHandler();
			if (!textureHandler)
				return;

			Dia::Graphics::FrameData& frameBuffer = simPU->GetRenderFrameBuffer();

			unsigned int redId = textureHandler->GetTextureId(Dia::Core::StringCRC("texture.test_red"));
			unsigned int blueId = textureHandler->GetTextureId(Dia::Core::StringCRC("texture.test_blue"));
			unsigned int greenId = textureHandler->GetTextureId(Dia::Core::StringCRC("texture.test_green"));

			if (redId != 0)
			{
				Dia::Graphics::SpriteDrawCommand redSprite(redId, Dia::Maths::Vector2D(200.0f, 200.0f));
				frameBuffer.RequestDrawSprite(redSprite);

				Dia::Graphics::SpriteDrawCommand transparentSprite(redId, Dia::Maths::Vector2D(200.0f, 300.0f));
				transparentSprite.tint = Dia::Graphics::RGBA(255, 255, 255, 128);
				frameBuffer.RequestDrawSprite(transparentSprite);
			}

			if (blueId != 0)
			{
				Dia::Graphics::SpriteDrawCommand blueSprite(blueId, Dia::Maths::Vector2D(300.0f, 200.0f));
				blueSprite.scale = Dia::Maths::Vector2D(1.5f, 1.5f);
				frameBuffer.RequestDrawSprite(blueSprite);
			}

			if (greenId != 0)
			{
				Dia::Graphics::SpriteDrawCommand greenSprite(greenId, Dia::Maths::Vector2D(400.0f, 200.0f));
				greenSprite.rotation = 45.0f;
				frameBuffer.RequestDrawSprite(greenSprite);
			}
		}
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _DummyStageSimRunningPhase = Cluiche::DummyStage::SimRunningPhase; }
DIA_REGISTER_PHASE(_DummyStageSimRunningPhase) {
	return new Cluiche::DummyStage::SimRunningPhase(pu);
}
