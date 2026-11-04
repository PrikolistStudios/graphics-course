#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"
#include "LandscapeShaderParams.h"

layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
//  vec3 wTangent;
  vec2 texCoord;
} surf;

layout(set = 0, binding = 0) uniform AppData
{
  UniformParams params;
};

layout(set = 1, binding = 1) uniform LandscapeData {
  LandscapeShaderParams landscape_params;
};

layout(set = 2, binding = 0) uniform sampler2D grass_bc;
layout(set = 2, binding = 1) uniform sampler2D grass_rh;
layout(set = 2, binding = 2) uniform sampler2D grass_hm;

layout(set = 2, binding = 3) uniform sampler2D rock_bc;
layout(set = 2, binding = 4) uniform sampler2D rock_rh;
layout(set = 2, binding = 5) uniform sampler2D rock_hm;

layout(set = 2, binding = 6) uniform sampler2D sand_bc;
layout(set = 2, binding = 7) uniform sampler2D sand_rh;
layout(set = 2, binding = 8) uniform sampler2D sand_hm;

layout(set = 2, binding = 9) uniform sampler2D snow_bc;
layout(set = 2, binding = 10) uniform sampler2D snow_rh;
layout(set = 2, binding = 11) uniform sampler2D snow_hm;



const float gamma = 2.2;
const float pi = 3.1415926;
const float FDiel = 0.04; // Fresnel for dielectrics

float D_GGX(float roughness, float nh)
{
  float m = roughness * roughness;
  float m2 = m * m;
  float nh2 = nh * nh;
  float d = (m2 - 1.0) * nh2 + 1.0;

  return m2 / (pi * d * d);
}

vec3 fresnel(vec3 f0, float product)
{
  product = clamp(product, 0.0, 1.0);

  return f0 + (1.0 - f0) * pow(1.0 - product, 5.0);
}

float G_neumann(float nl, float nv)
{
  return nl * nv / max(nl, nv);
}

vec3 cookTorrance(float nl, float nv, float nh, float vh, vec3 f0, float roughness)
{
  float D = D_GGX(roughness, nh);
  float G = G_neumann(nl, nv);

  return vec3(f0 * D * G);
}

vec4 blendTerrain(
    vec2 uv,
    float world_height,
    vec3 world_normal,
    sampler2D grass_tex,
    sampler2D sand_tex,
    sampler2D rock_tex,
    sampler2D snow_tex
) {
  const float SEA_LEVEL = -0.5;
  const float SAND_LEVEL = 0.1;
  const float SAND_BLEND_RANGE = 0.0;

  const float SNOW_HEIGHT = 2.0;
  const float SNOW_BLEND_RANGE = 0.3;

  const float SLOPE_ROCK_MAX = 0.65;
  const float SLOPE_GRASS_MIN = 0.85;

  vec4 grass_color = texture(grass_tex, uv);
  vec4 sand_color = texture(sand_tex, uv);
  vec4 rock_color = texture(rock_tex, uv);
  vec4 snow_color = texture(snow_tex, uv);

  const vec3 UP = vec3(0.0, 0.0, 1.0);
  float flatness = clamp(dot(normalize(world_normal), UP), 0.0, 1.0);

  float rock_factor = 1.0 - smoothstep(SLOPE_ROCK_MAX, SLOPE_GRASS_MIN, flatness);
  vec4 mid_elevation_color = mix(grass_color, rockColor, rock_factor);

  float sand_factor = 1.0 - smoothstep(SEA_LEVEL, SAND_LEVEL + SAND_BLEND_RANGE, world_height);
  vec4 low_to_mid_factor = mix(mid_elevation_color, sand_color, sand_factor);

  float snow_factor = smoothstep(SNOW_HEIGHT - SNOW_BLEND_RANGE, SNOW_HEIGHT, world_height);

  return mix(low_to_mid_factor, snow_color, snow_factor);
}

float sampleHeight(vec2 uv) {
  return blendTerrain(uv, surf.wPos.z, surf.wNorm, grass_hm, sand_hm, rock_hm, snow_hm).x;
}

vec3 generateNormal(vec2 uv, vec3 N) {
  // Generate the normal from heightmap first;
  ivec2 texture_size = textureSize(grass_hm, 0);
  float texel_size = 1.0 / float(texture_size.x);
  vec3 off = vec3(texel_size, texel_size, 0.0);
  float hL = sampleHeight(uv - off.xz);
  float hR = sampleHeight(uv + off.xz);
  float hD = sampleHeight(uv - off.zy);
  float hU = sampleHeight(uv + off.zy);
  float bump_intensity = 5.0;

  vec3 localN;
  localN.x = (hL - hR) * bump_intensity;
  localN.y = (hD - hU) * bump_intensity;
  localN.z = 1.0;
  localN = normalize(localN);

  // Now transform the normal to the world stace using TBN.
  // The terrain is axis-algned now so this is normal.
  vec3 T = normalize(vec3(1.0, 0.0, 0.0) - N * N.x);
  vec3 B = cross(N, T);
  mat3 TBN = mat3(T, B, N);
  return normalize(TBN * localN);
}

void main()
{
  vec3 dir = normalize(params.cam_pos.xyz - surf.wPos);
  vec3 light = params.lightPos.xyz;

  vec3 p = surf.wPos;
  vec3 n = generateNormal(surf.texCoord, surf.wNorm);
  vec3 v = dir;
  vec3 l = normalize(light - p);
  vec3 h = normalize(l + v);

  vec3 base = blendTerrain(surf.texCoord, p.z, surf.wNorm, grass_bc, sand_bc, rock_bc, snow_bc).xyz;
  base = pow(base, vec3(gamma)) * 30.0;

  float roughness = 0.9;
  float metallness = 0.0;

  float nv = max(0.0, dot(n, v));
  float nl = max(0.0, dot(n, l));
  float nh = max(0.0, dot(n, h));
  float hl = max(0.0, dot(h, l));
  float vh = max(0.0, dot(h, v));

  vec3 F0 = mix(vec3(FDiel), base, metallness);
  vec3 specFresnel = fresnel(F0, vh);
  vec3 spec = cookTorrance(nl, nv, nh, vh, specFresnel, roughness) * nl / max(0.001, 4.0 * nl * nv);
  vec3 diff = (vec3(1.0) - specFresnel) * nl / pi;
  vec3 lightColor = vec3(1.0);

  vec4 color = vec4((diff * mix(base, vec3(0.0), metallness) + spec) * lightColor, 1.0);
  vec4 gammaColor = pow(color, vec4(1.0 / gamma));
  out_fragColor = gammaColor;
}
