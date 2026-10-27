//
// Created by ev-ka on 26.10.2026.
//

#include "Landscape.hpp"

#include "etna/Profiling.hpp"

#include <imgui.h>

Landscape::Landscape()
  : noise_params({.octave_count = 6})
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

  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = sceneMgr->getVertexFormatDescription(),
    }},
  };
}

void Landscape::render(vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderLandscape);

  noise_gen.render(cmd_buf, noise_tx);
  etna::flush_barriers(cmd_buf);
  quadRenderer->render(cmd_buf, target_image, target_image_view, noise_tx, defaultSampler);
}

void Landscape::drawGui()
{
  int32_t octaveCount = noise_params.octave_count;
  ImGui::SliderInt("Octave Count", &octaveCount, 1, 8);
  noise_params.octave_count = octaveCount;

  float lacunarity = noise_params.lacunarity;
  ImGui::SliderFloat("Lacunarity", &lacunarity, 1.5f, 2.5f);
  noise_params.lacunarity = lacunarity;

  float gain = noise_params.gain;
  ImGui::SliderFloat("Gain", &gain, 0.3f, 0.8f);
  noise_params.gain = gain;

  int32_t seed = noise_params.seed;
  ImGui::InputInt("Seed", &seed);
  noise_params.seed = seed;

  float falloffStart = noise_params.faloff_start;
  ImGui::SliderFloat("Falloff Start", &falloffStart, 0.0f, 1.0f);
  noise_params.faloff_start = falloffStart;

  float falloffEnd = noise_params.faloff_end;
  ImGui::SliderFloat("Falloff End", &falloffEnd, 0.0f, 1.0f);
  noise_params.faloff_end = falloffEnd;

  noise_gen.update(noise_params);
}
