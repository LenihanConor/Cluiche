#include <DiaUtilityAI/PersonalityProfile.h>

namespace Dia { namespace UtilityAI {

PersonalityProfile::PersonalityProfile()
    : mEvalPeriodTicks(1), mValid(false)
{}

PersonalityProfile PersonalityProfile::LoadFromJson(const Json::Value& root)
{
    PersonalityProfile p;
    if (!root.isObject()) return p;

    if (root.isMember("name") && root["name"].isString())
        p.mName = Dia::Core::StringCRC(root["name"].asCString());

    if (root.isMember("eval_period_ticks") && root["eval_period_ticks"].isInt())
    {
        int period = root["eval_period_ticks"].asInt();
        p.mEvalPeriodTicks = (period > 0) ? period : 1;
    }

    if (root.isMember("biases") && root["biases"].isArray())
    {
        const Json::Value& biasArray = root["biases"];
        for (Json::ArrayIndex i = 0; i < biasArray.size() && !p.mBiases.IsFull(); ++i)
        {
            const Json::Value& b = biasArray[i];
            ActionBias bias;
            if (b.isMember("action") && b["action"].isString())
                bias.actionId = Dia::Core::StringCRC(b["action"].asCString());
            if (b.isMember("multiplier") && b["multiplier"].isNumeric())
                bias.scoreMultiplier = b["multiplier"].asFloat();
            p.mBiases.Add(bias);
        }
    }

    p.mValid = true;
    return p;
}

Dia::Core::StringCRC PersonalityProfile::GetName() const { return mName; }

float PersonalityProfile::GetScoreMultiplier(Dia::Core::StringCRC actionId) const
{
    for (unsigned int i = 0; i < mBiases.Size(); ++i)
    {
        if (mBiases[i].actionId == actionId)
            return mBiases[i].scoreMultiplier;
    }
    return 1.0f;  // FD-002: unlisted = 1.0f
}

int  PersonalityProfile::GetEvalPeriodTicks() const { return mEvalPeriodTicks; }
bool PersonalityProfile::IsValid() const { return mValid; }

}} // namespace Dia::UtilityAI
