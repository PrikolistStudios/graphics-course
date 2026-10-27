//
// Created by ev-ka on 25.10.2026.
//

#ifndef GRAPHICS_COURSE_PERLINNOISEGENERATOR_HPP
#define GRAPHICS_COURSE_PERLINNOISEGENERATOR_HPP

#include "etna/ComputePipeline.hpp"
#include "etna/Sampler.hpp"

#include <etna/Image.hpp>
#include <glm/vec2.hpp>

#include "shaders/NoiseParams.h"

class PerlinNoiseGenerator
{
public:
  struct Params
  {
    glm::uint octave_count = 6;
    float lacunarity = 2.0;
    float gain = 0.5;
    glm::uint seed = 0;
    float faloff_start = 0.4;
    float faloff_end = 0.5;
  };

  explicit PerlinNoiseGenerator(const Params&);

  void init();
  etna::Image createImage(glm::uvec2 resolution);
  void update(const Params&);
  void render(vk::CommandBuffer cmd_buf, const etna::Image& target_image);

private:
  void convertParams(const Params& params);
  void storeBuffer();

  NoiseParams params;
  etna::Buffer params_buf;
  etna::Sampler sampler;
  etna::ComputePipeline pipeline;
};


#endif // GRAPHICS_COURSE_PERLINNOISEGENERATOR_HPP
