#include "Modules/Scene2DModule.h"

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaScene2D/SceneLoadContext.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC Scene2DModule::kTypeId("Scene2DModule");

Scene2DModule::Scene2DModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult Scene2DModule::DoStart()
{
    DIA_LOG_INFO("Application", "Scene2DModule::DoStart");

    const Dia::Core::Path::Alias sceneAlias("stage_scene");
    if (!Dia::Core::PathStore::IsPathAliasRegistered(sceneAlias))
    {
        DIA_LOG_ERROR("Application", "Scene2DModule: 'stage_scene' alias not registered — no scene declared in .diastage");
        return Dia::ApplicationFlow::StartResult::kFailed;
    }

    auto* entityModule = mEntityRef.Get();
    auto* cameraModule = mCameraRef.Get();
    auto* lightModule  = mLightRef.Get();

    if (!entityModule)
    {
        DIA_LOG_ERROR("Application", "Scene2DModule: EntityModule not available — declare it as a dependency in the stage .diaapp");
        return Dia::ApplicationFlow::StartResult::kFailed;
    }
    if (!cameraModule)
    {
        DIA_LOG_ERROR("Application", "Scene2DModule: Camera2DModule not available — declare it as a dependency in the stage .diaapp");
        return Dia::ApplicationFlow::StartResult::kFailed;
    }
    if (!lightModule)
    {
        DIA_LOG_ERROR("Application", "Scene2DModule: Light2DModule not available — declare it as a dependency in the stage .diaapp");
        return Dia::ApplicationFlow::StartResult::kFailed;
    }

    const char* scenePath = Dia::Core::PathStore::ResolvePathToCString(sceneAlias);

    Dia::Scene2D::SceneLoadContext context{
        cameraModule->GetRegistry(),
        lightModule->GetRegistry(),
        entityModule->GetDomain()
    };
    Dia::Scene2D::SceneLoadErrors errors;

    mLoaded = mSceneLoader.Load(scenePath, context, mLayerTable, &errors);

    if (!mLoaded)
    {
        DIA_LOG_ERROR("Application", "Scene2DModule: scene load failed — '%s'", scenePath);
        return Dia::ApplicationFlow::StartResult::kFailed;
    }

    DIA_LOG_INFO("Application", "Scene2DModule: loaded '%s'", scenePath);
    return Dia::ApplicationFlow::StartResult::kReady;
}

Dia::ApplicationFlow::StopResult Scene2DModule::DoStop()
{
    DIA_LOG_INFO("Application", "Scene2DModule::DoStop");

    auto* entityModule = mEntityRef.Get();
    auto* cameraModule = mCameraRef.Get();
    auto* lightModule  = mLightRef.Get();

    if (entityModule && cameraModule && lightModule)
    {
        Dia::Scene2D::SceneLoadContext context{
            cameraModule->GetRegistry(),
            lightModule->GetRegistry(),
            entityModule->GetDomain()
        };
        mSceneLoader.Unload(context);
    }

    mLoaded = false;
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

namespace { using Scene2DModule_ = Cluiche::AppFlow::Scene2DModule; }
DIA_MODULE(Scene2DModule_);
DIA_DESCRIBE(Scene2DModule_::kTypeId, "Loads a .diascene into sibling module registries; owns only LayerTable and loader state.");
