#include "Modules/LogPushBatcher.h"

namespace Cluiche { namespace AppFlow {

LogPushBatcher::Plan LogPushBatcher::Evaluate(double nowMs, uint64_t cursor, uint64_t totalWritten) const
{
    Plan plan;
    if (totalWritten <= cursor)
        return plan;
    if (mHasPushed && (nowMs - mLastPushMs) < mMinIntervalMs)
        return plan;

    const uint64_t pending = totalWritten - cursor;
    plan.push    = true;
    plan.take    = pending > mMaxLines ? mMaxLines : static_cast<unsigned>(pending);
    plan.dropped = pending - plan.take;
    plan.startCursor = cursor + plan.dropped;
    return plan;
}

} } // namespace Cluiche::AppFlow
