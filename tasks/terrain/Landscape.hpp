//
// Created by ev-ka on 26.10.2026.
//

#ifndef GRAPHICS_COURSE_LANDSCAPERENDERER_HPP
#define GRAPHICS_COURSE_LANDSCAPERENDERER_HPP

#include "PerlinNoiseGenerator.hpp"

#include "render_utils/QuadRenderer.hpp"

class Landscape
{
public:
  Landscape();
  void init(vk::Format swapchain_format);
  void render(vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);
  void drawGui();

private:
  std::unique_ptr<QuadRenderer> quadRenderer;
  etna::Sampler defaultSampler;
  PerlinNoiseGenerator::Params noise_params;
  PerlinNoiseGenerator noise_gen;
  etna::Image noise_tx;
};


#endif // GRAPHICS_COURSE_LANDSCAPERENDERER_HPP
