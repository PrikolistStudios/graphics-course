#version 430

layout(location = 0) in vec2 viewUV;

layout(location = 0) out vec4 sideColor;
layout(location = 1) out vec4 centerColor;

layout(push_constant) uniform PushConstants
{
  ivec2 resolution;
  float time;
} pc;

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

vec3 gradient(uint h) {
  const vec3 gradients[12] = vec3[12](
      vec3(1, 1, 0), vec3(-1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0),
      vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
      vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, 1, -1), vec3(0, -1, -1)
  );
  return gradients[int(h % 12u)];
}

float interpolate(float value1, float value2, float value3, float value4, float value5, float value6, float value7, float value8, vec3 t) {
  return mix(
      mix(mix(value1, value2, t.x), mix(value3, value4, t.x), t.y),
      mix(mix(value5, value6, t.x), mix(value7, value8, t.x), t.y),
      t.z
  );
}

vec3 fade(vec3 t) {
  // 6t^5 - 15t^4 + 10t^3
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float perlinNoise(vec3 position, uint seed) {
  vec3 floorPosition = floor(position);
  vec3 fractPosition = position - floorPosition;
  uvec3 cellCoordinates = uvec3(ivec3(floorPosition));
  float value1 = dot(gradient(hash(cellCoordinates, seed)), fractPosition);
  float value2 = dot(gradient(hash(cellCoordinates + uvec3(1, 0, 0), seed)), fractPosition - vec3(1, 0, 0));
  float value3 = dot(gradient(hash(cellCoordinates + uvec3(0, 1, 0), seed)), fractPosition - vec3(0, 1, 0));
  float value4 = dot(gradient(hash(cellCoordinates + uvec3(1, 1, 0), seed)), fractPosition - vec3(1, 1, 0));
  float value5 = dot(gradient(hash(cellCoordinates + uvec3(0, 0, 1), seed)), fractPosition - vec3(0, 0, 1));
  float value6 = dot(gradient(hash(cellCoordinates + uvec3(1, 0, 1), seed)), fractPosition - vec3(1, 0, 1));
  float value7 = dot(gradient(hash(cellCoordinates + uvec3(0, 1, 1), seed)), fractPosition - vec3(0, 1, 1));
  float value8 = dot(gradient(hash(cellCoordinates + uvec3(1, 1, 1), seed)), fractPosition - vec3(1, 1, 1));
  return interpolate(value1, value2, value3, value4, value5, value6, value7, value8, fade(fractPosition));
}

float perlinNoise(vec3 position, int octaveCount, float persistence, float lacunarity, uint seed) {
  float value = 0.0;
  float amplitude = 1.0;
  for (int i = 0; i < octaveCount; i++) {
    uint s = hash(uint(i), seed);
    value += perlinNoise(position, s) * amplitude;
    amplitude *= persistence;
    position *= lacunarity;
  }
  return value;
}

vec4 generateSide(vec2 fragCoord) {
  float fireSpeed = 1.0;
  vec3 fireColor = vec3(1.0, 0.0, 0.0);

  vec2 leftUV = fragCoord / pc.resolution.y;
  leftUV.x += pc.time * fireSpeed;
  uint seed1 = 42u;
  float value1 = perlinNoise(vec3(leftUV, 1.0), 6, 0.5, 2.0, seed1);
  value1 = (value1 + 1.0) * 0.5;

  uint seed2 = 69u;
  vec2 rightUV = fragCoord / pc.resolution.y;
  rightUV.x -= pc.time * fireSpeed;
  float value2 = perlinNoise(vec3(rightUV, 1.0), 6, 0.5, 2.0, seed2);
  value2 = (value2 + 1.0) * 0.5;

  float xRemapped = fragCoord.x / pc.resolution.x;
  float valueMult = abs(xRemapped);
  float fireIntensity = 3.0;

  return vec4(fireIntensity * valueMult * fireColor * value1, 1.0);
}

float eyeWidth(float y) {
  float eyeHeight = 0.5;
  return max(cos(y / eyeHeight), 0.0) / 3.8;
}

vec4 generateCenter(vec2 fragCoord) {
  float fireSpeed = 1.0;
  vec3 fireColor = vec3(1.0, 0.0, 0.0);

  vec2 leftUV = fragCoord / pc.resolution.y;
  leftUV.x += pc.time * fireSpeed;
  uint seed1 = 42u;
  float value1 = perlinNoise(vec3(leftUV, 1.0), 6, 0.5, 2.0, seed1);
  value1 = (value1 + 1.0) * 0.5;

  uint seed2 = 69u;
  vec2 rightUV = fragCoord / pc.resolution.y;
  rightUV.x -= pc.time * fireSpeed;
  float value2 = perlinNoise(vec3(rightUV, 1.0), 6, 0.5, 2.0, seed2);
  value2 = (value2 + 1.0) * 0.5;

  float xRemapped = (fragCoord.x / pc.resolution.x - 0.5) * 2.0;
  float valueMult = 1.0 - abs(xRemapped);
  float fireIntensity = 2.0;

  vec4 res;
  if (fragCoord.x > pc.resolution.x / 2.0) {
    res = vec4(fireIntensity * valueMult * fireColor * value2, 1.0);
  } else {
    res = vec4(fireIntensity * valueMult * fireColor * value1, 1.0);
  }

  float yRemapped = (fragCoord.y / pc.resolution.y - 0.5) * 2.0;
  if (abs(xRemapped) < eyeWidth(yRemapped) / 2.0) {
    res = vec4(vec3(0.0, 0.0, 0.0), 1.0);
  }

  return res;
}

void main() {
  vec2 fragCoord = viewUV * vec2(pc.resolution);
  centerColor = generateCenter(fragCoord);
  sideColor = generateSide(fragCoord);
}
