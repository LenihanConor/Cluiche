#pragma once

#include <Types/StageLaunchRequest.h>
#include <DiaCore/Json/external/json/json.h>

#include <string>

namespace Cluiche { namespace AppFlow {

class StageCatalogueState;

// Reply builders for the stage catalogue queries (StageCatalogue F1 AC 15). Pure: state + request args -> reply.
// Shapes match `dia stages list|get|results --json` (Dia/DiaCLI/dia_cli/utils/stage_catalogue_view.py).
// Errors are {"error": "<message>"}.
namespace StageCatalogueQueries {

static constexpr const char* kListName = "stages.list";
static constexpr const char* kGetName = "stages.get";
static constexpr const char* kResultsName = "stages.results";
static constexpr const char* kLaunchName = "stages.launch";

static constexpr const char* kListHelp =
    "List catalogue stages as {stages: [{name, kinds, tags, description, presets, last_result}]}. "
    "Args: tag (array of strings or one string; all must match), kind (showcase|tool|test|other).";
static constexpr const char* kGetHelp =
    "One stage as {name, kinds, tags, description, presets, last_result, proves}. Args: stage (required). Unknown stage returns {error}.";
static constexpr const char* kLaunchHelp =
    "Launch a stage (optionally with a preset) from Boot. Args: stage (required), preset (optional, must be a preset of the stage). "
    "Replies {ok:true} once queued; {error} for an unknown stage or preset, or when the app is not at Boot.";
static constexpr const char* kResultsHelp =
    "Last results ({stage: {severity, label, utc, session_id, log_path}}), or one stage's result (null when none) with arg stage. "
    "Unknown stage returns {error}.";

Json::Value List(const StageCatalogueState& state, const Json::Value& args);
Json::Value Get(const StageCatalogueState& state, const Json::Value& args);
Json::Value Results(const StageCatalogueState& state, const Json::Value& args);

// Validates a stages.launch request against the catalogue. `atBoot` = the application is currently at the Boot stage.
// On success fills `out`; on failure returns false with a message in `error` (the module replies {error}).
bool ValidateLaunch(const StageCatalogueState& state, const Json::Value& args, bool atBoot, StageLaunchRequest& out, std::string& error);

} // namespace StageCatalogueQueries

}} // namespace Cluiche::AppFlow
