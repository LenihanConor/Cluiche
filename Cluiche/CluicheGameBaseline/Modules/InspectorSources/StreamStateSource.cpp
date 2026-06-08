#include "Modules/InspectorSources/StreamStateSource.h"

namespace Cluiche { namespace AppFlow {

StreamStateSource::StreamStateSource(Dia::ApplicationFlow::IApplicationInspectable* app)
    : mApp(app)
{
}

Dia::Core::StringCRC StreamStateSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("app.streams");
    return kTopic;
}

Dia::DebugServer::SourcePolicy StreamStateSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kChangeDetected, 0.0f, 0, 0.0f };
}

unsigned int StreamStateSource::CollectAndHash(Json::Value& payload)
{
    if (!mApp) return 0;

    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::StreamInfo, 16> streams;
    mApp->GetStreamInfo(streams);
    if (streams.Size() == 0) return 0;

    unsigned int hash = streams.Size();
    Json::Value streamsArray(Json::arrayValue);

    for (unsigned int i = 0; i < streams.Size(); ++i)
    {
        hash = HashCombine(hash, static_cast<unsigned int>(streams[i].id.Value()));
        hash = HashCombine(hash, static_cast<unsigned int>(streams[i].currentSequence & 0xFFFFFFFF));

        Json::Value entry;
        entry["streamId"]    = streams[i].id.AsChar();
        entry["msgPerSec"]   = 0;
        entry["kbPerSec"]    = 0.0;
        entry["fillPercent"] = 0.0;
        entry["dropsTotal"]  = 0;
        streamsArray.append(entry);
    }

    payload["streams"] = streamsArray;
    return hash;
}

}} // namespace Cluiche::AppFlow
