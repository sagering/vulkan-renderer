
#include "playground.h"

void
RenderGraph::Setup()
{
  OnSetupVirtualImages();
  OnSetupRenderPasses();

  for (auto pass : passes) {
    pass.second->OnBeforeBuild(this);
  }

  for (auto kv : images) {
    AnalyzeVirtualImage(kv.second);
  }

  for (auto pass : passes) {
    pass.second->Build(device, this);
    pass.second->OnAfterBuild(this);
  }

  OnSetupPhysicalImages();
}
