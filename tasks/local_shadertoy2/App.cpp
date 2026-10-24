#include "App.hpp"

#include "etna/RenderTargetStates.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>


void App::loadShaders()
{
  etna::create_program(outShaderName.data(), {LOCAL_SHADERTOY2_SHADERS_ROOT "toy.frag.spv", LOCAL_SHADERTOY2_SHADERS_ROOT "toy.vert.spv"});
  etna::create_program(textureShaderName.data(), {LOCAL_SHADERTOY2_SHADERS_ROOT "texture.frag.spv", LOCAL_SHADERTOY2_SHADERS_ROOT "toy.vert.spv"});
}

void App::setupPipelines()
{
  auto& pipelineManager = etna::get_context().getPipelineManager();
  mainPipeline = pipelineManager.createGraphicsPipeline(
    outShaderName.data(),
    etna::GraphicsPipeline::CreateInfo{
      // We don't care about this, because we will not provide any vertices.
      .vertexShaderInput = {},
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          // Disabling the culling is intentional.
          .cullMode = vk::CullModeFlagBits::eNone,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vkWindow->getCurrentFormat()},
        },
    });

  std::array<vk::PipelineColorBlendAttachmentState, 2> blendAttachmentStates{};

  blendAttachmentStates[0].blendEnable = VK_FALSE;
  blendAttachmentStates[0].colorWriteMask =
    vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  blendAttachmentStates[1].blendEnable = VK_FALSE;
  blendAttachmentStates[1].colorWriteMask =
    vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  texturePipeline = pipelineManager.createGraphicsPipeline(
    textureShaderName.data(),
    etna::GraphicsPipeline::CreateInfo{
      // We don't care about this, because we will not provide any vertices.
      .vertexShaderInput = {},
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          // Disabling the culling is intentional.
          .cullMode = vk::CullModeFlagBits::eNone,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      // Need to set logic op for some reason, even though it is disabled.
      .blendingConfig = {.attachments = {blendAttachmentStates[0], blendAttachmentStates[1]}, .logicOpEnable = false, .logicOp = vk::LogicOp::eNoOp},
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {sideTexture.getFormat(), sideTexture.getFormat()},
      }});
}

void App::setupImages()
{
  centerTexture = etna::get_context().createImage(
    {.extent = vk::Extent3D{resolution.x, resolution.y, 1u},
     .name = "output_image",
     .format = vk::Format::eR8G8B8A8Unorm,
     .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment});

  sideTexture = etna::get_context().createImage(
    {.extent = vk::Extent3D{resolution.x, resolution.y, 1u},
     .name = "output_image",
     .format = vk::Format::eR8G8B8A8Unorm,
     .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment});
}

App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    std::vector deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    etna::initialize(
      etna::InitParams{
        .applicationName = "Local Shadertoy",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .instanceExtensions = instanceExtensions,
        .deviceExtensions = deviceExtensions,
        // Replace with an index if etna detects your preferred GPU incorrectly
        .physicalDeviceIndexOverride = {},
        .numFramesInFlight = 1,
      });
  }

  commandManager = etna::get_context().createPerFrameCmdMgr();

  osWindow = windowing.createWindow(
    OsWindow::CreateInfo{
      .resolution = resolution,
    });

  {
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    vkWindow = etna::get_context().createWindow(
      etna::Window::CreateInfo{
        .surface = std::move(surface),
      });

    auto [w, h] = vkWindow->recreateSwapchain(
      etna::Window::DesiredProperties{
        .resolution = {resolution.x, resolution.y},
        .vsync = useVsync,
        .numFramesInFlight = static_cast<uint32_t>(commandManager->getCmdBufferCount()),
      });

    resolution = {w, h};
  }

  loadShaders();

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  setupImages();
  setupPipelines();
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  startTime = std::chrono::steady_clock::now();

  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    drawFrame();
  }

  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::renderHelperTexture(vk::CommandBuffer currentCmdBuf)
{
  currentCmdBuf.pushConstants<PushConstantData>(texturePipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, {pushConstants});
  etna::set_state(
    currentCmdBuf,
    sideTexture.get(),
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    currentCmdBuf,
    centerTexture.get(),
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::RenderTargetState renderTargets(
    currentCmdBuf,
    {{0, 0}, {resolution.x, resolution.y}},
    {
      {.image = sideTexture.get(), .view = sideTexture.getView({})},
      {.image = centerTexture.get(), .view = centerTexture.getView({})},
    },
    {});

  currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, texturePipeline.getVkPipeline());
  currentCmdBuf.draw(3, 1, 0, 0);
}

void App::renderScreen(vk::CommandBuffer currentCmdBuf, vk::Image backbuffer, vk::ImageView backbufferView)
{
  currentCmdBuf.pushConstants<PushConstantData>(mainPipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, {pushConstants});

  etna::set_state(
    currentCmdBuf,
    backbuffer,
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlagBits2::eTransferWrite,
    vk::ImageLayout::eTransferDstOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    currentCmdBuf,
    sideTexture.get(),
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    currentCmdBuf,
    centerTexture.get(),
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);

  auto simpleMaterialInfo = etna::get_shader_program(outShaderName.data());

  auto set = etna::create_descriptor_set(
    simpleMaterialInfo.getDescriptorLayoutId(0),
    currentCmdBuf,
    {etna::Binding{0, sideTexture.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
     etna::Binding{1, centerTexture.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}});

  etna::RenderTargetState renderTargets(currentCmdBuf, {{0, 0}, {resolution.x, resolution.y}}, {{.image = backbuffer, .view = backbufferView}}, {});

  currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, mainPipeline.getVkPipeline());
  currentCmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mainPipeline.getVkPipelineLayout(), 0, {set.getVkSet()}, {});
  etna::flush_barriers(currentCmdBuf);

  currentCmdBuf.draw(3, 1, 0, 0);

  etna::set_state(
    currentCmdBuf,
    backbuffer,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    {},
    vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(currentCmdBuf);
}

void App::updatePushConstants()
{
  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<float> elapsed = now - startTime;
  pushConstants.time = elapsed.count();
  pushConstants.resolution = resolution;
}

void App::drawFrame()
{
  auto currentCmdBuf = commandManager->acquireNext();

  etna::begin_frame();

  auto nextSwapchainImage = vkWindow->acquireNext();

  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem, backbufferReadyForPresentSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      updatePushConstants();
      renderHelperTexture(currentCmdBuf);
      renderScreen(currentCmdBuf, backbuffer, backbufferView);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    auto renderingDone = commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem), std::move(backbufferReadyForPresentSem));

    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(
      etna::Window::DesiredProperties{
        .resolution = {resolution.x, resolution.y},
        .vsync = useVsync,
        .numFramesInFlight = static_cast<uint32_t>(commandManager->getCmdBufferCount()),
      });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
