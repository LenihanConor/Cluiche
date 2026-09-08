#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaScene2D/SceneLoader2D.h>
#include <DiaScene2D/LayerTable.h>
#include "Modules/EntityModule.h"
#include "Modules/Camera2DModule.h"
#include "Modules/Light2DModule.h"

namespace Cluiche { namespace AppFlow {

class Scene2DModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Loads a .diascene into sibling module registries; owns only LayerTable and loader state.";

    explicit Scene2DModule(const Dia::Core::StringCRC& instanceId);

    const Dia::Scene2D::LayerTable& GetLayerTable() const { return mLayerTable; }
    bool                            IsLoaded()      const { return mLoaded; }

protected:
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override {}
    Dia::ApplicationFlow::StopResult  DoStop()           override;

private:
    Dia::Scene2D::SceneLoader2D mSceneLoader;
    Dia::Scene2D::LayerTable    mLayerTable;
    bool                        mLoaded = false;

    Dia::ApplicationFlow::ModuleRef<EntityModule>   mEntityRef{this};
    Dia::ApplicationFlow::ModuleRef<Camera2DModule> mCameraRef{this};
    Dia::ApplicationFlow::ModuleRef<Light2DModule>  mLightRef{this};

    // Cached at DoStart so DoStop can call Unload after sibling modules leave kActive.
    Dia::Camera2D::CameraRegistry2D*   mCachedCameraRegistry = nullptr;
    Dia::Lighting2D::LightRegistry2D*  mCachedLightRegistry  = nullptr;
    Dia::Entity::Domain*               mCachedEntityDomain   = nullptr;
};

} } // namespace Cluiche::AppFlow
