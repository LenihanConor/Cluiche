#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>

namespace Dia { namespace Economy {

    // Forward declaration — include EconomySchema.h in the .cpp only
    class EconomySchema;

    // -----------------------------------------------------------------------
    // ResourcePool — runtime state for a single resource slot
    // -----------------------------------------------------------------------
    struct ResourcePool
    {
        Dia::Core::StringCRC resource_name;
        float                value;
        float                minimum;
        float                maximum;
        float                income_accumulator;
        float                last_tick_income;   // gross income applied since last Tick() reset
        float                last_tick_spend;    // gross spend applied since last Tick() reset
    };

    // -----------------------------------------------------------------------
    // EconomyInstance — runtime pool state for one economy participant
    // -----------------------------------------------------------------------
    class EconomyInstance
    {
    public:
        EconomyInstance();
        ~EconomyInstance();

        EconomyInstance(const EconomyInstance& other);
        EconomyInstance& operator=(const EconomyInstance& other);

        // Factory methods — create an instance from a schema, optionally
        // applying per-resource starting_value overrides from a JSON file.
        [[nodiscard]] static EconomyInstance CreateFromSchema(const EconomySchema& schema);
        [[nodiscard]] static EconomyInstance CreateFromJson(const char* override_json_path,
                                                            const EconomySchema& schema);

        // --- public query API ---
        float                GetValue(Dia::Core::StringCRC resource_name)   const;
        float                GetMaximum(Dia::Core::StringCRC resource_name) const;
        float                GetMinimum(Dia::Core::StringCRC resource_name) const;
        bool                 HasResource(Dia::Core::StringCRC resource_name) const;
        Dia::Core::StringCRC GetInstanceName() const;

        // --- income accumulator (fractional carry-over) ---
        float GetIncomeAccumulator(Dia::Core::StringCRC resource_name) const;

        // --- tick rate tracking ---
        float GetLastTickIncome(Dia::Core::StringCRC resource_name) const;
        float GetLastTickSpend(Dia::Core::StringCRC resource_name) const;

        // --- schema access ---
        const EconomySchema* GetSchema() const;

        // --- package-internal mutators (called by EconomySystem) ---
        void SetValue_Internal(Dia::Core::StringCRC resource_name, float value);
        void SetIncomeAccumulator_Internal(Dia::Core::StringCRC resource_name, float value);
        void AddLastTickIncome_Internal(Dia::Core::StringCRC resource_name, float amount);
        void AddLastTickSpend_Internal(Dia::Core::StringCRC resource_name, float amount);
        void ResetLastTickRates_Internal();

    private:
        Dia::Core::Containers::DynamicArray<ResourcePool> mPools;
        Dia::Core::StringCRC                              mInstanceName;
        const EconomySchema*                              mSchema;
    };

}} // namespace Dia::Economy
