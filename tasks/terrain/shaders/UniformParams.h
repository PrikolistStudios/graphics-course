#ifndef UNIFORM_PARAMS_H_INCLUDED
#define UNIFORM_PARAMS_H_INCLUDED

#include "cpp_glsl_compat.h"

struct UniformParams
{
  shader_mat4 lightMatrix;
  shader_vec4 lightPos;
  shader_vec4 baseColor;
  shader_vec4 cam_pos;
  shader_float time;
  float _pad[3];
};


#endif // UNIFORM_PARAMS_H_INCLUDED
