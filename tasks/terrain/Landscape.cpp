//
// Created by ev-ka on 26.10.2026.
//

#include "Landscape.hpp"

#include "etna/PipelineManager.hpp"
#include "etna/Profiling.hpp"
#include "etna/RenderTargetStates.hpp"

#include <imgui.h>

Landscape::Landscape(const Params& params)
  : oneShotCommands{etna::get_context().createOneShotCmdMgr()}
  , transferHelper{etna::BlockingTransferHelper::CreateInfo{.stagingSize = 4096 * 4096 * 4}}
  , params(params)
  , newCellNum(params.cell_num)
  , noise_params({.octave_count = 6})
  , noise_gen(noise_params)
  , noise_tx()
{
}

void Landscape::init(vk::Format swapchain_format)
{
  quadRenderer = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{
    .format = swapchain_format,
    .rect = {{0, 0}, {512, 512}},
  });

  noise_gen.init();
  noise_tx = noise_gen.createImage({1024, 1024});
  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});

  etna::create_program("Landscape Shader", {TERRAIN_SHADERS_ROOT "terrain.vert.glsl.spv", TERRAIN_SHADERS_ROOT "terrain.frag.glsl.spv"});

  pipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "Landscape Shader",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = {},
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eNone,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {swapchain_format},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });

  regenerateIBuf();
}

void Landscape::render(const RenderContext& ctx)
{
  ETNA_PROFILE_GPU(ctx.cmd_buf, renderLandscape);

  {
    noise_gen.render(ctx.cmd_buf, noise_tx);
    etna::flush_barriers(ctx.cmd_buf);

    auto shaderInfo = etna::get_shader_program("Landscape Shader");

    etna::set_state(
      ctx.cmd_buf,
      noise_tx.get(),
      vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2::eShaderSampledRead,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eColor);

    auto set = etna::create_descriptor_set(
      shaderInfo.getDescriptorLayoutId(1),
      ctx.cmd_buf,
      {etna::Binding{0, noise_tx.genBinding(defaultSampler.get(), vk::ImageLayout::eReadOnlyOptimal, {})},
       etna::Binding{1, vert_shader_params_buf.genBinding()}});

    etna::RenderTargetState renderTargets(
      ctx.cmd_buf,
      {{0, 0}, {ctx.swapchain_resolution.x, ctx.swapchain_resolution.y}},
      {{.image = ctx.target_image, .view = ctx.target_image_view}},
      {.image = ctx.main_view_depth.get(), .view = ctx.main_view_depth.getView({})});

    ctx.cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
    ctx.cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipelineLayout(), 1, {set.getVkSet()}, {});
    ctx.cmd_buf.bindIndexBuffer(index_buffer.get(), 0, vk::IndexType::eUint32);

    etna::flush_barriers(ctx.cmd_buf);

    ctx.cmd_buf.drawIndexed(indices.size(), 1, 0, 0, 0);
  }

  if (drawNoiseTex)
  {
    quadRenderer->render(ctx.cmd_buf, ctx.target_image, ctx.target_image_view, noise_tx, defaultSampler);
  }
}

void Landscape::drawGui()
{
  ImGui::Indent();

  if (ImGui::CollapsingHeader("Noise"))
  {
    noise_gen.drawGUI("Landscape Noise");
  }
  ImGui::Checkbox("Draw Debug Noise Texture", &drawNoiseTex);

  bool changed = false;
  changed |= ImGui::DragFloat("Height", &params.height, 0.1f, 0.f, 100.f);
  changed |= ImGui::DragFloat("Size", &params.size, 0.5f, 0.f, 1000.f);
  if (changed)
  {
    updateVertShader();
  }

  ImGui::Separator();
  ImGui::SliderInt("Cell Number", &newCellNum, 2, 1024);
  if (ImGui::Button("Regenerate Cells") && newCellNum != static_cast<int>(params.cell_num))
  {
    params.cell_num = newCellNum;
    regenerateIBuf();
  }

  ImGui::Unindent();
}

void Landscape::updateVertShader()
{
  LandscapeShaderParams shaderData = {.world_height = params.height, .world_size = params.size, .cell_num = params.cell_num};

  if (!vert_shader_params_buf.get())
  {
    vert_shader_params_buf = etna::get_context().createBuffer(
      etna::Buffer::CreateInfo{
        .size = sizeof(shaderData),
        .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
        .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .name = "Landscape Shader Data Buffer",
      });
  }

  std::memcpy(vert_shader_params_buf.data(), &shaderData, sizeof(shaderData));
}

void Landscape::regenerateIBuf()
{
  indices.clear();
  for (uint32_t i = 0; i < params.cell_num; i++)
  {
    for (uint32_t j = 0; j < params.cell_num; j++)
    {
      uint32_t globalIndex = i * (params.cell_num + 1) + j;
      uint32_t topLeft = globalIndex;
      uint32_t topRight = globalIndex + 1;
      uint32_t bottomLeft = globalIndex + 1 + params.cell_num;
      uint32_t bottomRight = globalIndex + 1 + params.cell_num + 1;

      // Upper triangle in the cell.
      indices.emplace_back(topRight);
      indices.emplace_back(topLeft);
      indices.emplace_back(bottomLeft);

      // Lower triangle in the cell.
      indices.emplace_back(topRight);
      indices.emplace_back(bottomLeft);
      indices.emplace_back(bottomRight);
    }
  }

  old_ind_buf = std::move(index_buffer);
  index_buffer = etna::get_context().createBuffer(
    etna::Buffer::CreateInfo{
      .size = indices.size() * sizeof(uint32_t),
      .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .name = "Landscape Index Buffer",
    });

  std::memcpy(index_buffer.data(), indices.data(), indices.size() * sizeof(uint32_t));

  updateVertShader();
}
