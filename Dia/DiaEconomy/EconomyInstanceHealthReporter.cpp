#include "DiaEconomy/EconomyInstanceHealthReporter.h"
#include "DiaEconomy/EconomyInstance.h"
#include "DiaEconomy/EconomySchema.h"

namespace Dia { namespace Economy {

EconomyInstanceHealthReporter::EconomyInstanceHealthReporter(
    Dia::Core::StringCRC reporterName, const EconomyInstance& instance)
    : mName(reporterName)
    , mInstance(instance)
{
    SetOK();
}

Dia::Core::StringCRC EconomyInstanceHealthReporter::GetReporterName() const
{
    return mName;
}

void EconomyInstanceHealthReporter::Check()
{
    const EconomySchema* schema = mInstance.GetSchema();
    if (!schema)
    {
        SetDegraded(Dia::Core::StringCRC("no_schema"));
        return;
    }

    const unsigned int count = schema->GetResourceCount();
    for (unsigned int i = 0; i < count; ++i)
    {
        const ResourceDefinition& def = schema->GetResourceByIndex(i);
        const float val = mInstance.GetValue(def.resource_name);
        const float mn  = mInstance.GetMinimum(def.resource_name);
        const float mx  = mInstance.GetMaximum(def.resource_name);

        if (val < mn || val > mx)
        {
            IncrementErrors();
            SetFailing(Dia::Core::StringCRC("pool_out_of_range"));
            return;
        }
    }

    SetOK();
}

}} // namespace Dia::Economy
