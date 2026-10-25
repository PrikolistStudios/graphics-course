//
// Created by ev-ka on 25.10.2026.
//

#include "PerlinNoiseGenerator.hpp"

#include "etna/Etna.hpp"
#include "etna/PipelineManager.hpp"

#include <glm/vec3.hpp>

PerlinNoiseGenerator::PerlinNoiseGenerator(const Params& params)
  : params(params)
{
}

void PerlinNoiseGenerator::init()
{
  etna::create_program("PerlinNoiseGenerator", {TERRAIN_SHADERS_ROOT "perlin.comp.glsl.spv"});
  pipeline = etna::get_context().getPipelineManager().createComputePipeline("PerlinNoiseGenerator", {});
  sampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "PerlinNoiseGenerator_sampler"});
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
    computeInfo.getDescriptorLayoutId(0), cmd_buf, {etna::Binding{0, target_image.genBinding(sampler.get(), vk::ImageLayout::eGeneral)}});

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, {set.getVkSet()}, {});

  glm::uvec2 resolution = {target_image.getExtent().width, target_image.getExtent().height};
  PushConstants pc = {
    .resolution = resolution,
    .invResolution = 1.0f / glm::vec2(resolution),
    .octave_count = params.octave_count,
    .lacunarity = params.lacunarity,
    .gain = params.gain,
    .seed = params.seed,
    .faloff_start = params.faloff_start,
    .faloff_end = params.faloff_end,
  };
  cmd_buf.pushConstants(pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(pc), &pc);

  etna::flush_barriers(cmd_buf);

  cmd_buf.dispatch((resolution.x + 31) / 32, (resolution.y + 31) / 32, 1);
}

etna::Image PerlinNoiseGenerator::createImage(glm::uvec2 resolution)
{
  return etna::get_context().createImage(
    {.extent = vk::Extent3D{resolution.x, resolution.y, 1u},
     .name = "perlin_noise",
     .format = vk::Format::eR32Sfloat,
     .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc});
}

void PerlinNoiseGenerator::updateParams(const Params& parameters)
{
  this->params = parameters;
}
