#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/IEntityInspectable.h>

namespace Cluiche { namespace AppFlow {

class EntityModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit EntityModule(const Dia::Core::StringCRC& instanceId);
    ~EntityModule() override;

    Dia::Entity::IEntityInspectable& GetInspectable() { return mDomain; }

protected:
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()           override;

private:
    Dia::Entity::Domain mDomain;
};

} } // namespace Cluiche::AppFlow
