//
// Created by ev-ka on 25.10.2026.
//

#include "PerlinNoiseGenerator.hpp"

#include "etna/Etna.hpp"
#include "etna/PipelineManager.hpp"

#include <glm/vec3.hpp>

PerlinNoiseGenerator::PerlinNoiseGenerator(const Params& params)
{
  convertParams(params);
}

void PerlinNoiseGenerator::init()
{
  params_buf = etna::get_context().createBuffer(
    etna::Buffer::CreateInfo{
      .size = sizeof(NoiseParams),
      .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .name = "Perlin Noise Generator Parameters Buffer",
    });

  etna::create_program("PerlinNoiseGenerator", {TERRAIN_SHADERS_ROOT "perlin.comp.glsl.spv"});
  pipeline = etna::get_context().getPipelineManager().createComputePipeline("PerlinNoiseGenerator", {});
  sampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "PerlinNoiseGenerator_sampler"});

  storeBuffer();
}

void PerlinNoiseGenerator::render(vk::CommandBuffer cmd_buf, const etna::Image& target_image)
{
  etna::set_state(
    cmd_buf,
    target_image.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderStorageWrite,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);

  auto computeInfo = etna::get_shader_program("PerlinNoiseGenerator");
  auto set = etna::create_descriptor_set(
    computeInfo.getDescriptorLayoutId(0),
    cmd_buf,
    {etna::Binding{0, target_image.genBinding(sampler.get(), vk::ImageLayout::eGeneral)}, etna::Binding{1, params_buf.genBinding()}});

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, {set.getVkSet()}, {});

  glm::uvec2 resolution = {target_image.getExtent().width, target_image.getExtent().height};
  if (resolution != params.resolution)
  {
    params.resolution = resolution;
    params.invResolution = 1.0f / glm::vec2(resolution);

    storeBuffer();
  }

  etna::flush_barriers(cmd_buf);

  cmd_buf.dispatch((params.resolution.x + 31) / 32, (params.resolution.y + 31) / 32, 1);
}

void PerlinNoiseGenerator::convertParams(const Params& parameters)
{
  params.lacunarity = parameters.lacunarity;
  params.octave_count = parameters.octave_count;
  params.gain = parameters.gain;
  params.seed = parameters.seed;
  params.falloff_start = parameters.faloff_start;
  params.falloff_end = parameters.faloff_end;
}

void PerlinNoiseGenerator::storeBuffer()
{
  std::memcpy(params_buf.data(), &params, sizeof(params));
}

etna::Image PerlinNoiseGenerator::createImage(glm::uvec2 resolution)
{
  if (resolution != params.resolution)
  {
    params.resolution = resolution;
    params.invResolution = 1.0f / glm::vec2(resolution);

    storeBuffer();
  }

  return etna::get_context().createImage(
    {.extent = vk::Extent3D{resolution.x, resolution.y, 1u},
     .name = "perlin_noise",
     .format = vk::Format::eR32Sfloat,
     .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc});
}

void PerlinNoiseGenerator::update(const Params& parameters)
{
  convertParams(parameters);
  storeBuffer();
}
