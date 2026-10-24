#pragma once

#include "etna/GraphicsPipeline.hpp"
#include "etna/Sampler.hpp"


#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"


class App
{
public:
  App();
  ~App();

  void run();

private:
  void loadShaders();
  void setupPipelines();
  void setupImages();
  void renderHelperTexture(vk::CommandBuffer currentCmdBuf);
  void renderScreen(vk::CommandBuffer currentCmdBuf, vk::Image backbuffer, vk::ImageView backbufferView);
  void updatePushConstants();

  void drawFrame();

  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  etna::GraphicsPipeline mainPipeline;
  etna::GraphicsPipeline texturePipeline;
  etna::Sampler defaultSampler;
  std::chrono::steady_clock::time_point startTime;
  etna::Image centerTexture;
  etna::Image sideTexture;

  struct PushConstantData
  {
    glm::uvec2 resolution;
    glm::float32 time;
  } pushConstants;

  const std::string_view outShaderName = "local_shadertoy2";
  const std::string_view textureShaderName = "side_texture";
};
