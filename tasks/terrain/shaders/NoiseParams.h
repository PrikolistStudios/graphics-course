//
// Created by ev-ka on 26.10.2026.
//

#ifndef GRAPHICS_COURSE_NOISEPARAMS_HPP
#define GRAPHICS_COURSE_NOISEPARAMS_HPP

#include "cpp_glsl_compat.h"

struct NoiseParams
{
  shader_uvec2 resolution;
  shader_vec2 invResolution;
  shader_uint octave_count;
  shader_float lacunarity;
  shader_float gain;
  shader_uint seed;
  shader_float falloff_start;
  shader_float falloff_end;
};

#endif // GRAPHICS_COURSE_NOISEPARAMS_HPP
