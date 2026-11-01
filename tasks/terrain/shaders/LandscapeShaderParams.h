//
// Created by ev-ka on 26.10.2026.
//

#ifndef GRAPHICS_COURSE_LANDSCAPEPARAMS_HPP
#define GRAPHICS_COURSE_LANDSCAPEPARAMS_HPP

#include "cpp_glsl_compat.h"

struct LandscapeShaderParams
{
  shader_float world_height;
  shader_float world_size;
  shader_uint cell_num;
};

#endif // GRAPHICS_COURSE_LANDSCAPEPARAMS_HPP
