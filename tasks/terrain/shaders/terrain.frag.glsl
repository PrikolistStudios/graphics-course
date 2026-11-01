#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"


layout(location = 0) out vec4 out_fragColor;

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
  vec3 light = params.lightPos - surf.wPos;
  vec3 l = normalize(light - surf.wPos);
  float nl = max(0.0, dot(surf.wNorm, l));
  out_fragColor = vec4(nl, nl, nl, 1.0);
}
