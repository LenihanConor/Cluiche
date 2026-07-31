#pragma once

namespace Dia
{
    namespace HTN
    {
        enum class TaskResult
        {
            kRunning,    // operator is still executing — call again next tick
            kSucceeded,  // operator completed — advance plan cursor
            kFailed      // operator failed — caller should re-plan
        };

    } // namespace HTN
} // namespace Dia
