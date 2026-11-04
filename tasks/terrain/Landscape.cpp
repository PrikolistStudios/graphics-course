//
// Created by ev-ka on 26.10.2026.
//

#include "Landscape.hpp"

#include "etna/PipelineManager.hpp"
#include "etna/Profiling.hpp"
#include "etna/RenderTargetStates.hpp"

// #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <imgui.h>
#include <iostream>

Landscape::Landscape(const Params& params)
  : one_shot_commands{etna::get_context().createOneShotCmdMgr()}
  , transfer_helper{etna::BlockingTransferHelper::CreateInfo{.stagingSize = 4096 * 4096 * 4}}
  , params(params)
  , newCellNum(params.cell_num)
  , noise_params({.octave_count = 3, .lacunarity = 1.620f, .gain = 0.405f, .faloff_start = 0.336f, .faloff_end = 0.495f})
  , noise_gen(noise_params)
  , noise_tx()
{
}

std::vector<etna::Binding> createTexturesBindings(const etna::Sampler& sampler, const std::vector<etna::Image*>& textures)
{
  std::vector<etna::Binding> res;
  for (size_t i = 0; i < textures.size(); i++)
  {
    res.emplace_back(i, textures[i]->genBinding(sampler.get(), vk::ImageLayout::eReadOnlyOptimal, {}));
  }

  return res;
}

void Landscape::init(vk::Format swapchain_format)
{
  quadRenderer = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{
    .format = swapchain_format,
    .rect = {{0, 0}, {512, 512}},
  });

  noise_gen.init();
  noise_tx = noise_gen.createImage({1024, 1024});

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eRepeat,
    .name = "default_sampler"
  });

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
  loadTextures();
  textures_bindings = createTexturesBindings(
    defaultSampler,
    {
      &grass.base_color,
      &grass.roughness,
      &grass.heightmap,
      &rock.base_color,
      &rock.roughness,
      &rock.heightmap,
      &sand.base_color,
      &sand.roughness,
      &sand.heightmap,
      &snow.base_color,
      &snow.roughness,
      &snow.heightmap,
    });
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

    auto texturesSet = etna::create_descriptor_set(shaderInfo.getDescriptorLayoutId(2), ctx.cmd_buf, textures_bindings);

    etna::RenderTargetState renderTargets(
      ctx.cmd_buf,
      {{0, 0}, {ctx.swapchain_resolution.x, ctx.swapchain_resolution.y}},
      {{.image = ctx.target_image, .view = ctx.target_image_view}},
      {.image = ctx.main_view_depth.get(), .view = ctx.main_view_depth.getView({})});

    ctx.cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
    ctx.cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipelineLayout(), 1, {set.getVkSet(), texturesSet.getVkSet()}, {});
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

void Landscape::loadTextures()
{
  grass.base_color = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/grass/sparse_grass_diff_1k.png", BaseColor);
  grass.roughness = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/grass/sparse_grass_rough_1k.png", Roughness);
  grass.heightmap = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/grass/sparse_grass_disp_1k.png", HeightMap);

  rock.base_color = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/rock/rock_06_diff_1k.png", BaseColor);
  rock.roughness = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/rock/rock_06_rough_1k.png", Roughness);
  rock.heightmap = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/rock/rock_06_disp_1k.png", HeightMap);

  sand.base_color = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/sand/sand_03_diff_1k.png", BaseColor);
  sand.roughness = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/sand/sand_03_rough_1k.png", Roughness);
  sand.heightmap = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/sand/sand_03_disp_1k.png", HeightMap);

  snow.base_color = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/snow/snow_01_diff_1k.png", BaseColor);
  snow.roughness = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/snow/snow_01_rough_1k.png", Roughness);
  snow.heightmap = loadTexture(GRAPHICS_COURSE_RESOURCES_ROOT "/terrain/snow/snow_01_disp_1k.png", HeightMap);
}

etna::Image Landscape::loadTexture(const std::string_view& path, TextureType texture_type)
{
  int desiredChannels = STBI_default;
  int bytesPerPixel = 4;
  vk::Format textureFormat = {};
  if (texture_type == BaseColor)
  {
    desiredChannels = STBI_rgb_alpha;
    textureFormat = vk::Format::eR8G8B8A8Srgb;
    bytesPerPixel = 4;
  }
  else if (texture_type == Roughness)
  {
    desiredChannels = STBI_rgb_alpha;
    textureFormat = vk::Format::eR8G8B8A8Srgb;
    bytesPerPixel = 4;
  }
  else if (texture_type == HeightMap)
  {
    desiredChannels = STBI_grey;
    textureFormat = vk::Format::eR16Unorm;
    bytesPerPixel = 2;
  }
  // desiredChannels = STBI_default;

  int texWidth, texHeight, texChannels;
  std::byte* pixels;

  if (texture_type == HeightMap)
  {
    pixels = reinterpret_cast<std::byte*>(stbi_load_16(path.data(), &texWidth, &texHeight, &texChannels, desiredChannels));
  }
  else
  {
    pixels = reinterpret_cast<std::byte*>(stbi_load(path.data(), &texWidth, &texHeight, &texChannels, desiredChannels));
  }

  if (pixels == nullptr)
  {
    throw std::runtime_error("Failed to load texture image!");
  }

  auto res = etna::get_context().createImage(
    {.extent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
     .name = path.data(),
     .format = textureFormat,
     .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
     .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY});

  auto pixelsSpan = std::span(pixels, texWidth * texHeight * bytesPerPixel);
  transfer_helper.uploadImage(*one_shot_commands, res, 0, 0, pixelsSpan);
  stbi_image_free(pixels);
  return res;
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
