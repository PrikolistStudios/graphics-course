#version 430
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

// Hand-made perlin noise.

#include "NoiseParams.h"

layout(local_size_x = 32, local_size_y = 32) in;

layout(set = 0, binding = 0, r32f) uniform image2D resultImage;

layout(set = 0, binding = 1) uniform NoiseData {
  NoiseParams pc;
};

//layout(push_constant) uniform PushConstants
//{
//  uvec2 resolution;
//  vec2 invResolution;
//  uint octave_count;
//  float lacunarity;
//  float gain;
//  uint seed;
//  float falloff_start;
//  float faloff_end;
//} pc;

const float noise_normalizer = sqrt(3.0) / 2.0;

// fast high-quality hash https://www.shadertoy.com/view/wfVczm
uint hash(uvec3 key, uint seed) {
  uvec3 k = key;
  k *= 0x27d4eb2fu;
  k ^= k >> 16;
  k *= 0x85ebca77u;
  uint h = seed;
  h ^= k.x;
  h ^= h >> 16;
  h *= 0x9e3779b1u;
  h ^= k.y;
  h ^= h >> 16;
  h *= 0x9e3779b1u;
  h ^= k.z;
  h ^= h >> 16;
  h *= 0x9e3779b1u;
  h ^= h >> 16;
  h *= 0xed5ad4bbu;
  h ^= h >> 16;
  return h;
}

// generates a distinct seed for each octave
// that will behave like a 4th coordinate
// when mixed into the final hash
uint hash(uint key, uint seed) {
  uint k = key;
  k *= 0x27d4eb2fu;
  k ^= k >> 16;
  k *= 0x85ebca77u;
  uint h = seed;
  h ^= k;
  h ^= h >> 16;
  h *= 0x9e3779b1u;
  return h;
}

float float_hash(uint int_hash) {
  uint mantissa = int_hash >> 9;
  uint b = 0x3F800000u | mantissa;
  return uintBitsToFloat(b) - 1.0;
  //  return int_hash * (1.0 / 4294967296.0);
}

float fade(float value) {
  float t3 = value * value * value;
  return 6 * t3 * value * value - 15 * t3 * value + 10 * t3;
}

vec3 gradient(uint h) {
  const vec3 gradients[12] = vec3[12](
      vec3(1, 1, 0), vec3(-1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0),
      vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
      vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, 1, -1), vec3(0, -1, -1)
  );
  return gradients[int(h % 12u)];
}

float perlin(vec3 P, uint seed) {
  uvec3 base = uvec3(floor(P));
  float influence[8];
  for (int i = 0; i < 8; ++i) {
    uvec3 corner = base + uvec3(i & 1, sign(i & 2), sign(i & 4));
    uint cornerHash = hash(corner, seed);
    //    float x = fma(float_hash(hash(corner, seed + 1)), 2.0, -1.0);
    //    float y = fma(float_hash(hash(corner, seed + 2)), 2.0, -1.0);
    //    float z = fma(float_hash(hash(corner, seed + 3)), 2.0, -1.0);
    //
    //    vec3 gradient = normalize(vec3(x, y, z));
    vec3 grad = normalize(gradient(cornerHash));
    vec3 offset = P - vec3(corner);
    influence[i] = dot(offset, grad);
  }

  vec3 frac = fract(P);
  vec3 w = vec3(fade(frac.x), fade(frac.y), fade(frac.z));
  float raw = mix(
      mix(mix(influence[0], influence[1], w.x), mix(influence[2], influence[3], w.x), w.y),
      mix(mix(influence[4], influence[5], w.x), mix(influence[6], influence[7], w.x), w.y),
      w.z
  );

  return raw / noise_normalizer;
}

void main() {
  if (gl_GlobalInvocationID.x > pc.resolution.x - 1 || gl_GlobalInvocationID.y > pc.resolution.y - 1) {
    return;
  }

  vec2 frag_coord = vec2(gl_GlobalInvocationID.xy);
  float base_uv_multiplier = 4.0;
  vec2 uv = frag_coord * pc.invResolution;

  float noise = 0;
  float max_weight = 0.0;
  for (int i = 0; i < pc.octave_count; ++i) {
    vec3 P = vec3(uv * base_uv_multiplier, i);
    P *= pow(pc.lacunarity, i);
    float cur_amp = pow(pc.gain, i);
    max_weight += cur_amp;
    noise += perlin(P, pc.seed + i) * cur_amp;
  }
  noise /= max_weight;
  noise = noise * 0.5 + 0.5;

  float d = length(uv - vec2(0.5, 0.5));
  float d_in = pc.falloff_start;
  float d_out = max(pc.falloff_start, pc.falloff_end);
  float t = clamp((d - d_in) / (d_out - d_in), 0.0, 1.0);
  float m = 1.0 - (3 * t * t - 2 * t * t * t);

  float result = noise * m;

  imageStore(resultImage, ivec2(frag_coord), vec4(result, 0.0, 0.0, 1.0));
}
