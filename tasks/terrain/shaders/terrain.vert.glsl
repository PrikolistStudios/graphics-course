#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.glsl"
#include "LandscapeShaderParams.h"

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
} params;

layout(set = 1, binding = 0) uniform sampler2D height_map;
layout(set = 1, binding = 1) uniform LandscapeData {
  LandscapeShaderParams landscape_params;
};


layout (location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
//  vec3 wTangent;
//  vec2 texCoord;
} vOut;

out gl_PerVertex { vec4 gl_Position; };

float height(vec2 P) {
  ivec2 texture_size = textureSize(height_map, 0);
  vec2 texPos = P / landscape_params.world_size;
  return (texture(height_map, texPos).x - 0.5) * landscape_params.world_height;
}

vec3 normal(vec2 P) {
  ivec2 texture_size = textureSize(height_map, 0);
  float texel_size = landscape_params.world_size / float(texture_size.x);
  vec3 off = vec3(texel_size, texel_size, 0.0);
  float hL = height(P.xy - off.xz);
  float hR = height(P.xy + off.xz);
  float hD = height(P.xy - off.zy);
  float hU = height(P.xy + off.zy);

  vec3 N;
  N.x = hL - hR;
  N.y = hD - hU;
  N.z = texel_size * 2.0;
  N = normalize(N);
  return N;
}

void main(void)
{
  //  const vec4 wTang = vec4(decode_normal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

  float vertex_delta = landscape_params.world_size / landscape_params.cell_num;
  int j = gl_VertexIndex % int(landscape_params.cell_num + 1);
  int i = (gl_VertexIndex - j) / int(landscape_params.cell_num + 1);
  vec3 P = vec3(vertex_delta * i, vertex_delta * j, 0.f);
  float h = height(P.xy);
  const vec4 wNorm = vec4(normal(P.xy), 0.0f);

  vOut.wPos = (params.mModel * vec4(P + vec3(0.0, 0.0, h), 1.0f)).xyz;
  vOut.wNorm = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
  //  vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
  //  vOut.texCoord = vTexCoordAndTang.xy;

  gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
}
