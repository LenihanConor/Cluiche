#include <DiaMetrics/Counter.h>

#include <algorithm>

namespace Dia
{
    namespace Metric
    {
        struct CounterShard
        {
            std::atomic<uint64_t> value{ 0 };
            Counter*              owner = nullptr;
        };

        static thread_local std::vector<std::pair<Counter*, CounterShard*>> tShards;

        Counter::Counter() = default;

        Counter::~Counter()
        {
            // Remove this counter's entry from the calling thread's shard cache.
            // Other threads' caches will find owner==nullptr and skip on next use.
            tShards.erase(
                std::remove_if(tShards.begin(), tShards.end(),
                    [this](const std::pair<Counter*, CounterShard*>& p) {
                        return p.first == this;
                    }),
                tShards.end());

            std::lock_guard<std::mutex> lock(mShardsMutex);
            for (CounterShard* s : mShards)
                delete s;
            mShards.clear();
        }

        CounterShard* Counter::GetOrCreateShard()
        {
            for (auto& [owner, shard] : tShards)
            {
                if (owner == this)
                    return shard;
            }

            CounterShard* shard = new CounterShard();
            shard->owner = this;

            {
                std::lock_guard<std::mutex> lock(mShardsMutex);
                mShards.push_back(shard);
            }

            tShards.emplace_back(this, shard);
            return shard;
        }

        void Counter::Inc(uint64_t delta)
        {
            GetOrCreateShard()->value.fetch_add(delta, std::memory_order_relaxed);
        }

        uint64_t Counter::Value() const
        {
            uint64_t total = 0;
            std::lock_guard<std::mutex> lock(mShardsMutex);
            for (const CounterShard* s : mShards)
                total += s->value.load(std::memory_order_relaxed);
            return total;
        }

    } // namespace Metric
} // namespace Dia
