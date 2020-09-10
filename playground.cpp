
#include "playground.h"

void
RenderGraph::Setup()
{
  for (auto workUnit : work) {
    workUnit->Build();
  }

  OnSetupPhysicalImages();
}

void
RenderGraph::RenderFrame()
{
  OnFrameResolvePhysicalImages();

  for (auto& kv : images) {
    auto vImage = kv.second;
    auto pImage = physicalImages[kv.first];
    vImage->BuildBarriers(pImage);
    vImage->ResetCounter();
  }

  VkDevice device = base->device;
  auto cmdBuffer = base->NextCmdBuffer();
  VkQueue queue = base->queue;
  VkSemaphore imageAvailableSemaphore = base->imageAvailableSemaphore;
  VkSemaphore renderFinishedSemaphore = base->renderFinishedSemaphore;

  VkCommandBufferBeginInfo beginInfo = vkiCommandBufferBeginInfo(nullptr);
  ASSERT_VK_SUCCESS(vkBeginCommandBuffer(cmdBuffer.cmdBuffer, &beginInfo));

  for (auto& workUnit : work) {
    workUnit->RecordCommands(cmdBuffer.cmdBuffer);
  }

  ASSERT_VK_SUCCESS(vkEndCommandBuffer(cmdBuffer.cmdBuffer));

  VkPipelineStageFlags waitStage =
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submitInfo = vkiSubmitInfo(1,
                                          &imageAvailableSemaphore,
                                          &waitStage,
                                          1,
                                          &cmdBuffer.cmdBuffer,
                                          1,
                                          &renderFinishedSemaphore);
  ASSERT_VK_SUCCESS(vkQueueSubmit(queue, 1, &submitInfo, cmdBuffer.fence));

  OnFrame();
}
