#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaScene2D/SceneLoader2D.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaEntity/Domain.h>

namespace Cluiche { namespace AppFlow {

class Scene2DModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Loads and owns a .diascene for the active stage; exposes registries and domain to sibling modules.";

    explicit Scene2DModule(const Dia::Core::StringCRC& instanceId);

    const Dia::Camera2D::CameraRegistry2D&  GetCameraRegistry() const { return mCameraRegistry; }
    const Dia::Lighting2D::LightRegistry2D& GetLightRegistry()  const { return mLightRegistry; }
    const Dia::Scene2D::LayerTable&         GetLayerTable()     const { return mLayerTable; }
    const Dia::Entity::Domain&              GetEntityDomain()   const { return mEntityDomain; }
    Dia::Entity::Domain&                    GetEntityDomain()         { return mEntityDomain; }
    bool                                    IsLoaded()          const { return mLoaded; }

protected:
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(float dt) override {}
    Dia::ApplicationFlow::StopResult  DoStop()           override;

private:
    Dia::Scene2D::SceneLoader2D        mSceneLoader;
    Dia::Scene2D::LayerTable           mLayerTable;
    Dia::Camera2D::CameraRegistry2D    mCameraRegistry;
    Dia::Lighting2D::LightRegistry2D   mLightRegistry;
    Dia::Entity::Domain                mEntityDomain;
    bool                               mLoaded = false;
};

} } // namespace Cluiche::AppFlow
