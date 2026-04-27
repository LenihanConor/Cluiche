#include "DiaPipelineEditor/PipelineEditorPlugin.h"
#include "DiaPipelineEditor/PipelineLogTailer.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>

using namespace Dia::PipelineEditor;

PipelineEditorPlugin::PipelineEditorPlugin()
	: mTailer(nullptr)
	, mBridge(nullptr)
{
}

PipelineEditorPlugin::~PipelineEditorPlugin()
{
}

void PipelineEditorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
{
	mBridge = context.mBridge;

	mTailer = new PipelineLogTailer();
	mTailer->Initialize("Cluiche/out/DiaCLI/logs/pipeline/last-run.ndjson");
}

void PipelineEditorPlugin::OnUnload()
{
	if (mTailer)
	{
		mTailer->Shutdown();
		delete mTailer;
		mTailer = nullptr;
	}
}

void PipelineEditorPlugin::OnUpdate(float /*deltaTime*/)
{
	if (mTailer)
	{
		mTailer->Poll();
	}
}

REGISTER_EDITOR_PLUGIN(PipelineEditorPlugin, "DiaPipelineEditor")
