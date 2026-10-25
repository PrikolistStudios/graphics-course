//
// Created by ev-ka on 25.10.2026.
//

#ifndef GRAPHICS_COURSE_PERLINNOISEGENERATOR_HPP
#define GRAPHICS_COURSE_PERLINNOISEGENERATOR_HPP

#include "etna/ComputePipeline.hpp"
#include "etna/Sampler.hpp"

#include <etna/Image.hpp>
#include <glm/vec2.hpp>

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
  void updateParams(const Params&);
  void render(vk::CommandBuffer cmd_buf, const etna::Image& target_image);

private:
  struct PushConstants
  {
    glm::uvec2 resolution;
    glm::vec2 invResolution;
    glm::uint octave_count;
    float lacunarity;
    float gain;
    glm::uint seed;
    float faloff_start;
    float faloff_end;
  };

  Params params;
  etna::Sampler sampler;
  etna::ComputePipeline pipeline;
};


#endif // GRAPHICS_COURSE_PERLINNOISEGENERATOR_HPP
