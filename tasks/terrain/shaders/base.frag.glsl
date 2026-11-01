#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
//  vec3 wTangent;
//  vec2 texCoord;
} surf;

layout(set = 0, binding = 0) uniform AppData
{
  UniformParams params;
};

void main()
{
}
