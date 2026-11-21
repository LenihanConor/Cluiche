#include "Types/PerfTripPublisher.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/TripRecord.h>

#include <memory>
#include <string>

namespace Cluiche { namespace AppFlow {

using Dia::Core::StringCRC;
using namespace Dia::Observation::Profile;

namespace
{
    const StringCRC& TopicCrc() { static const StringCRC c(PerfTripPublisher::TopicName()); return c; }

    // One trip -> JSON via the trips.jsonl serialiser, so the keys are exactly the file schema.
    bool BuildTripJson(const TripRecord& record, Json::Value& out)
    {
        // Trips are rate limited (>= 5 s per unit), so a heap buffer per published trip is fine.
        std::unique_ptr<char[]> buf(new char[16384]);
        size_t len = 0;
        SerializeTripRecord(record, buf.get(), 16384, len);
        if (len == 0) return false;
        Json::Reader reader;
        return reader.parse(buf.get(), buf.get() + len, out) && out.isObject();
    }
}

uint32_t DebugServerPerfTripSink::SubscriberCount() const
{
    return mServer.GetTopicSubscriberCount(TopicCrc());
}

void DebugServerPerfTripSink::PublishOrdered(const Json::Value& payload)
{
    mServer.PublishTopicOrdered(TopicCrc(), payload);
}

PerfTripPublisher::PerfTripPublisher(Dia::ApplicationFlow::EventStreamStore<TripChunk>* store)
    : mStore(store)
    , mReader(-1)
    , mSink(nullptr)
    , mPublished(0)
{
    if (mStore)
    {
        mReader = mStore->RegisterReader();
        if (mReader < 0)
            DIA_LOG_WARNING("PerfTrip", "No free reader on $perftrips; perf.trip will have no data");
        else
            mScratch = std::make_unique<DrainBuffer>();
    }
}

void PerfTripPublisher::Drain()
{
    if (!mStore || mReader < 0 || !mScratch) return;
    // Bounded: at most the stream capacity worth of chunks per tick; leftovers wait for the next tick.
    for (int pass = 0; pass < 32; ++pass)
    {
        mScratch->RemoveAll();
        mStore->Consume(mReader, *mScratch);
        if (mScratch->Size() == 0) return;
        if (!mSink || mSink->SubscriberCount() == 0)
            continue;   // consumed and discarded: nothing is built without a subscriber
        for (unsigned int i = 0; i < mScratch->Size(); ++i)
        {
            Json::Value json;
            if (!BuildTripJson((*mScratch)[i].payload.record, json)) continue;
            mSink->PublishOrdered(json);
            ++mPublished;
        }
    }
}

}} // namespace Cluiche::AppFlow
