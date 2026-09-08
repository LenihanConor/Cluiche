#pragma once

#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Log/DiaLog.h>

#define DIA_OBSERVATION_ASSERT(reporter, cond, reason_crc, msg)  \
	do {                                                          \
		if (!(cond)) {                                            \
			DIA_LOG_ERROR("scenario", "%s", msg);                \
			(reporter)->SetFailing(reason_crc);                  \
		}                                                         \
	} while (0)

#define DIA_OBSERVATION_FAIL(reporter, reason_crc, msg)          \
	do {                                                          \
		DIA_LOG_ERROR("scenario", "%s", msg);                    \
		(reporter)->SetFailing(reason_crc);                      \
	} while (0)
