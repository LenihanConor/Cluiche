#include "DiaEconomy/EconomyInstance.h"
#include "DiaEconomy/EconomySchema.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Core/Assert.h>

#include <fstream>

namespace Dia { namespace Economy {

    // -----------------------------------------------------------------------
    // Construction / destruction
    // -----------------------------------------------------------------------
    EconomyInstance::EconomyInstance()
        : mPools()
        , mInstanceName()
        , mSchema(nullptr)
    {}

    EconomyInstance::~EconomyInstance()
    {}

    EconomyInstance::EconomyInstance(const EconomyInstance& other)
        : mPools()
        , mInstanceName(other.mInstanceName)
        , mSchema(other.mSchema)
    {
        const unsigned int count = other.mPools.Size();
        if (count > 0)
        {
            mPools.Reserve(count);
            for (unsigned int i = 0; i < count; ++i)
                mPools.Add(other.mPools[i]);
        }
    }

    EconomyInstance& EconomyInstance::operator=(const EconomyInstance& other)
    {
        if (this == &other)
            return *this;
        mInstanceName = other.mInstanceName;
        mSchema       = other.mSchema;
        const unsigned int count = other.mPools.Size();
        mPools.RemoveAll();
        if (count > 0)
        {
            if (mPools.Capacity() < count)
                mPools.Reserve(count);
            for (unsigned int i = 0; i < count; ++i)
                mPools.Add(other.mPools[i]);
        }
        return *this;
    }

    // -----------------------------------------------------------------------
    // CreateFromSchema
    // -----------------------------------------------------------------------
    EconomyInstance EconomyInstance::CreateFromSchema(const EconomySchema& schema)
    {
        EconomyInstance instance;
        instance.mSchema = &schema;

        const unsigned int count = schema.GetResourceCount();
        if (count > 0)
        {
            instance.mPools.Reserve(count);
            for (unsigned int i = 0; i < count; ++i)
            {
                const ResourceDefinition& def = schema.GetResourceByIndex(i);
                ResourcePool pool;
                pool.resource_name      = def.resource_name;
                pool.value              = def.starting_value;
                pool.minimum            = def.minimum_value;
                pool.maximum            = def.maximum_value;
                pool.income_accumulator = 0.0f;
                pool.last_tick_income   = 0.0f;
                pool.last_tick_spend    = 0.0f;
                instance.mPools.Add(pool);
            }
        }

        DIA_LOG_INFO("Economy", "EconomyInstance created: %s", instance.mInstanceName.AsChar());
        return instance;
    }

    // -----------------------------------------------------------------------
    // CreateFromJson
    // -----------------------------------------------------------------------
    EconomyInstance EconomyInstance::CreateFromJson(const char* override_json_path,
                                                    const EconomySchema& schema)
    {
        DIA_ASSERT(override_json_path != nullptr,
                   "EconomyInstance::CreateFromJson: override_json_path must not be null");

        // Start from a fully schema-initialised instance
        EconomyInstance instance = CreateFromSchema(schema);

        std::ifstream file(override_json_path);
        DIA_ASSERT(file.is_open(), "EconomyInstance::CreateFromJson: could not open JSON file");
        if (!file.is_open())
            return instance;

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors))
        {
            DIA_ASSERT(false, "EconomyInstance::CreateFromJson: JSON parse error");
            return instance;
        }

        // Read optional instance_name
        if (root.isMember("instance_name") && root["instance_name"].isString())
            instance.mInstanceName = Dia::Core::StringCRC(root["instance_name"].asCString());

        // Apply per-resource starting_value overrides
        if (root.isMember("overrides") && root["overrides"].isArray())
        {
            const Json::Value& overrides = root["overrides"];
            const unsigned int overrideCount = static_cast<unsigned int>(overrides.size());
            for (unsigned int i = 0; i < overrideCount; ++i)
            {
                const Json::Value& item = overrides[i];
                if (!item.isMember("resource_name") || !item["resource_name"].isString())
                    continue;
                if (!item.isMember("starting_value") || !item["starting_value"].isNumeric())
                    continue;

                const Dia::Core::StringCRC name(item["resource_name"].asCString());
                const float               overrideValue = item["starting_value"].asFloat();
                instance.SetValue_Internal(name, overrideValue);
            }
        }

        DIA_LOG_INFO("Economy", "EconomyInstance created: %s", instance.mInstanceName.AsChar());
        return instance;
    }

    // -----------------------------------------------------------------------
    // Public query API
    // -----------------------------------------------------------------------
    float EconomyInstance::GetValue(Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
                return mPools[i].value;
        }
        return 0.0f;
    }

    float EconomyInstance::GetMaximum(Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
                return mPools[i].maximum;
        }
        return 0.0f;
    }

    float EconomyInstance::GetMinimum(Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
                return mPools[i].minimum;
        }
        return 0.0f;
    }

    bool EconomyInstance::HasResource(Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
                return true;
        }
        return false;
    }

    Dia::Core::StringCRC EconomyInstance::GetInstanceName() const
    {
        return mInstanceName;
    }

    // -----------------------------------------------------------------------
    // Income accumulator
    // -----------------------------------------------------------------------
    float EconomyInstance::GetIncomeAccumulator(Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
                return mPools[i].income_accumulator;
        }
        return 0.0f;
    }

    // -----------------------------------------------------------------------
    // Schema access
    // -----------------------------------------------------------------------
    const EconomySchema* EconomyInstance::GetSchema() const
    {
        return mSchema;
    }

    // -----------------------------------------------------------------------
    // Package-internal mutators
    // -----------------------------------------------------------------------
    void EconomyInstance::SetValue_Internal(Dia::Core::StringCRC resource_name, float value)
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
            {
                ResourcePool& pool = mPools[i];
                // Clamp to [minimum, maximum]
                if (value < pool.minimum)
                    value = pool.minimum;
                if (value > pool.maximum)
                    value = pool.maximum;
                pool.value = value;
                return;
            }
        }
    }

    void EconomyInstance::SetIncomeAccumulator_Internal(Dia::Core::StringCRC resource_name,
                                                        float value)
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
            {
                mPools[i].income_accumulator = value;
                return;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Tick rate tracking — getters
    // -----------------------------------------------------------------------
    float EconomyInstance::GetLastTickIncome(Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
                return mPools[i].last_tick_income;
        }
        return 0.0f;
    }

    float EconomyInstance::GetLastTickSpend(Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
                return mPools[i].last_tick_spend;
        }
        return 0.0f;
    }

    // -----------------------------------------------------------------------
    // Tick rate tracking — internal mutators
    // -----------------------------------------------------------------------
    void EconomyInstance::AddLastTickIncome_Internal(Dia::Core::StringCRC resource_name, float amount)
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
            {
                mPools[i].last_tick_income += amount;
                return;
            }
        }
    }

    void EconomyInstance::AddLastTickSpend_Internal(Dia::Core::StringCRC resource_name, float amount)
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            if (mPools[i].resource_name == resource_name)
            {
                mPools[i].last_tick_spend += amount;
                return;
            }
        }
    }

    void EconomyInstance::ResetLastTickRates_Internal()
    {
        for (unsigned int i = 0; i < mPools.Size(); ++i)
        {
            mPools[i].last_tick_income = 0.0f;
            mPools[i].last_tick_spend  = 0.0f;
        }
    }

}} // namespace Dia::Economy
