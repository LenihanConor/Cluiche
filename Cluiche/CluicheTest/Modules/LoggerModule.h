#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Logger { class ISink; } }

namespace Cluiche { namespace AppFlow {

class LoggerModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit LoggerModule(const Dia::Core::StringCRC& instanceId);
    ~LoggerModule();

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    static constexpr unsigned int kMaxSinks = 8;
    Dia::Logger::ISink* mOwnedSinks[kMaxSinks];
    unsigned int mOwnedSinkCount = 0;
};

} } // namespace Cluiche::AppFlow
