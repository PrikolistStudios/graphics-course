#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.glsl"
#include "LandscapeShaderParams.h"

layout(location = 0) in vec3 vPos;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
} params;

layout (location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
//  vec3 wTangent;
//  vec2 texCoord;
} vOut;

out gl_PerVertex { vec4 gl_Position; };

void main(void)
{
  vOut.wPos = (params.mModel * vec4(vPos, 1.0f)).xyz;
}
