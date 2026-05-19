#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace Dia
{
    namespace Metric
    {
        struct CounterShard;

        class Counter
        {
        public:
            Counter();
            ~Counter();

            void     Inc(uint64_t delta = 1);
            uint64_t Value() const;

        private:
            friend struct CounterShard;

            CounterShard* GetOrCreateShard();

            mutable std::mutex           mShardsMutex;
            std::vector<CounterShard*>   mShards;
        };

    } // namespace Metric
} // namespace Dia
