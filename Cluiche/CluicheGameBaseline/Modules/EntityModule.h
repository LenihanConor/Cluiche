#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/IEntityInspectable.h>

namespace Cluiche { namespace AppFlow {

class EntityModule : public Dia::ApplicationFlow::SimModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit EntityModule(const Dia::Core::StringCRC& instanceId);
    ~EntityModule() override;

    Dia::Entity::IEntityInspectable& GetInspectable() { return mDomain; }
    Dia::Entity::Domain&             GetDomain()       { return mDomain; }
    const Dia::Entity::Domain&       GetDomain() const { return mDomain; }
    bool                             IsReady() const  { return mReady; }

protected:
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop()           override;

private:
    Dia::Entity::Domain mDomain;
    bool                mReady = false;
};

} } // namespace Cluiche::AppFlow
