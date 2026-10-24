#version 430

layout(location = 0) in vec2 viewUV;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
  ivec2 resolution;
  float time;
} pc;

layout(set = 0, binding = 0) uniform sampler2D sideTexture;
layout(set = 0, binding = 1) uniform sampler2D centerTexture;

// Helper functions.

mat3 rotateEuler(vec3 ang) {
  vec3 c = cos(ang), s = sin(ang);

  mat3 rx = mat3(1.0, 0.0, 0.0, 0.0, c.x, -s.x, 0.0, s.x, c.x);
  mat3 ry = mat3(c.y, 0.0, s.y, 0.0, 1.0, 0.0, -s.y, 0.0, c.y);
  mat3 rz = mat3(c.z, -s.z, 0.0, s.z, c.z, 0.0, 0.0, 0.0, 1.0);

  return rz * ry * rx;
}

mat3 rotateAxis(vec3 axis, float a) {
  float c = cos(a);
  float s = sin(a);
  vec3 v = normalize(axis);
  return mat3(
      c + v.x * v.x * (1.0 - c), v.y * v.x * (1.0 - c) + v.z * s, v.z * v.x * (1.0 - c) - v.y * s,
      v.x * v.y * (1.0 - c) - v.z * s, c + v.y * v.y * (1.0 - c), v.z * v.y * (1.0 - c) + v.x * s,
      v.x * v.z * (1.0 - c) + v.y * s, v.y * v.z * (1.0 - c) - v.x * s, c + v.z * v.z * (1.0 - c)
  );
}

mat4 translate(vec3 offset) {
  return mat4(
      1.0, 0.0, 0.0, 0.0,  // Column 0
      0.0, 1.0, 0.0, 0.0,  // Column 1
      0.0, 0.0, 1.0, 0.0,  // Column 2
      offset.x, offset.y, offset.z, 1.0   // Column 3
  );
}

// Objects.

float sphere(in vec3 p)
{
  float radius = 2.0;
  return length (p) - radius;
}

float box(in vec3 pos)
{
  mat3 transform = rotateEuler(vec3(radians(30.), radians(pc.time * 30.), 0.0));
  vec3 size = vec3(0.5, 0.5, 0.5);
  vec3 pt = abs(transform * pos) - size;

  return length(max(pt, 0.0)) + min(max(pt.x, max(pt.y, pt.z)), 0.0);
}

float sdCone(vec3 p, vec2 c, float h)
{
  // c is the sin/cos of the angle, h is height
  // Alternatively pass q instead of (c,h),
  // which is the point at the base in 2D
  vec2 q = h * vec2(c.x / c.y, -1.0);

  vec2 w = vec2(length(p.xz), p.y);
  vec2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
  vec2 b = w - q * vec2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
  float k = sign(q.y);
  float d = min(dot(a, a), dot(b, b));
  float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
  return sqrt(d) * sign(s);
}

float sdConeNew(vec3 p, vec2 q)
{
  vec2 w = vec2(length(p.xz), p.y);
  vec2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
  vec2 b = w - q * vec2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
  float k = sign(q.y);
  float d = min(dot(a, a), dot(b, b));
  float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
  return sqrt(d) * sign(s);
}

float dot2(in vec2 v) { return dot(v, v); }
float sdCappedCone(vec3 p, float h, float r1, float r2) {
  vec2 q = vec2(length(p.xz), p.y);
  vec2 k1 = vec2(r2, h);
  vec2 k2 = vec2(r2 - r1, 2.0 * h);
  vec2 ca = vec2(q.x - min(q.x, (q.y < 0.0)?r1:r2), abs(q.y) - h);
  vec2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot2(k2), 0.0, 1.0);
  float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
  return s * sqrt(min(dot2(ca), dot2(cb)));
}

float sdRoundCone(vec3 p, float r1, float r2, float h)
{
  float b = (r1 - r2) / h;
  float a = sqrt(1.0 - b * b);

  vec2 q = vec2(length(p.xz), p.y);
  float k = dot(q, vec2(-b, a));
  if (k < 0.0) return length(q) - r1;
  if (k > a * h) return length(q - vec2(0.0, h)) - r2;
  return dot(q, vec2(a, b)) - r1;
}

float rocketOld(vec3 pos, vec3 center) {
  mat3 transform = rotateEuler(vec3(0.0, 0.0, 0.0));
  vec3 size = vec3(0.25, 0.7, 0.25);
  vec3 pt = abs(pos - center) - size;
  float r = 0.1;
  return length(max(pt, 0.0)) + min(max(pt.x, max(pt.y, pt.z)), 0.0) - r;
}

float rocket(vec3 pos, vec3 center) {
  float angle = radians(69.0);
  float baseHeight = 1.0;
  float width = 0.4;
  float tipHeight = 0.7;
  float tip = sdConeNew(pos - (center + vec3(0.0, baseHeight + tipHeight, 0.0)), vec2(width, -tipHeight));

  float base = sdCappedCone(pos - center, baseHeight, width * 0.7, width);

  // p.xz = abs(p.xz);
  float footheight = 0.4;
  vec3 footPos = pos - center;
  footPos.xz = abs(footPos.xz);
  footPos -= vec3(0.4, -baseHeight - footheight, 0.4);
  footPos = rotateEuler(vec3(0.0, radians(-45.0), radians(30.0))) * footPos;

  float foot = sdRoundCone(footPos, 0.1, 0.2, footheight);

  return min(foot, min(base, tip));
}

float hash(int n) {
  return (fract(sin(float(n)) * 43758.5453123) - 0.5) * 2.0;
}

// Could randomize particle count, size, vertical speed, etc. but too lazy.

float explosion(vec3 pos, float time, int numParticles, int seed) {
  float res = 100000.0;
  for (int i = 0; i < numParticles; ++i) {
    int pSeed = seed + i * 432;
    float speed = hash(pSeed) + 1.0;
    vec2 horDir = vec2(hash(pSeed + 1), hash(pSeed + 2)) * 6.0;

    vec3 pPos;
    pPos.xz = pos.xz + horDir * speed * time;
    pPos.y = pos.y - 10.0 * time + 9.8 * time * time;

    float particle = sphere(pPos);

    res = min(res, particle);
  }

  return res;
}

float rocketAltitude(float time) {
  time = clamp(time, 0.0, 10000000.0);
  return time * time * time * 4.0;
}

#define EXPLOSION_TIME 3.0
float rocketLine(vec3 pos, vec3 start, float delta, int num) {
  float res = 100000.0;
  for (int i = 0; i < num; ++i) {
    float timeSinceLaunch = clamp(pc.time - 0.25 * float(i), 0.0, 10000000.0);

    float next;
    if (timeSinceLaunch < EXPLOSION_TIME) {
      vec3 center = start + vec3(delta * float(i), rocketAltitude(timeSinceLaunch), 0.0);
      next = rocket(pos, center);
    } else {
      vec3 center = start + vec3(delta * float(i), rocketAltitude(EXPLOSION_TIME), 0.0);
      next = explosion(pos - center, timeSinceLaunch - EXPLOSION_TIME, 10, 3321 * i);
    }

    res = min(res, next);
  }

  return res;
}

// Main logic.

float surface(vec3 p) {
  // return rocket(p, vec3(0.0, 0.0, 0.0));
  //return rocketLine(p, vec3(-2.0, 0.0, 0.0), 2.0, 3);
  return sphere(p);
}

#define EPS 0.001
vec3 generateNormal (vec3 z, float d) {
  float e = max(d * 0.5, EPS);
  float dx1 = surface(z + vec3(e, 0, 0));
  float dx2 = surface(z - vec3(e, 0, 0));
  float dy1 = surface(z + vec3(0, e, 0));
  float dy2 = surface(z - vec3(0, e, 0));
  float dz1 = surface(z + vec3(0, 0, e));
  float dz2 = surface(z - vec3(0, 0, e));

  return normalize(vec3(dx1 - dx2, dy1 - dy2, dz1 - dz2));
}

vec3 getTriplanarWeights(in vec3 n) {
  vec3 w = abs (n);

  w *= w;

  return w / (w.x + w.y + w.z);
}

#define MAX_ITERS 90
#define MAX_DIST 10000.0
vec3 trace (vec3 from, vec3 dir, out bool hit) {
  vec3 p = from;
  float totalDist = 0.0;

  hit = false;

  for (int steps = 0; steps < MAX_ITERS; steps++)
  {
    float dist = surface(p);

    if (dist < 0.001)
    {
      hit = true;
      break;
    }

    totalDist += dist;

    if (totalDist > MAX_DIST)
    break;

    p += dist * dir;
  }

  return p;
}

float phong(vec3 p, vec3 lightPos) {
  vec3  l = normalize(lightPos - p);
  vec3  n = generateNormal(p, 0.001);
  float nl = max(0.0, dot(n, l));
  return nl;
}

vec3 blinnPhong(vec3 p, vec3 lightPos, vec3 viewDir, vec3 ambientColor, vec3 surfaceColor, vec3 shineColor, float shininess) {
  vec3  l = normalize(lightPos - p);
  vec3  n = generateNormal(p, 0.001);

  vec3 ambient = ambientColor;

  vec3 diffuse = surfaceColor * max(0.0, dot(l, n));

  vec3 h = normalize(l + viewDir);
  vec3 specular = shineColor * pow(max(0.0, dot(n, h)), shininess);
  vec3 res = ambient + diffuse + specular;

  return res;
}

const float gamma = 2.2;
const float pi = 3.1415926;
const float FDiel = 0.04;       // Fresnel for dielectrics

float D_GGX(float roughness, float nh)
{
  float m = roughness * roughness;
  float m2 = m * m;
  float nh2 = nh * nh;
  float d = (m2 - 1.0) * nh2 + 1.0;

  return m2 / (pi * d * d);
}

vec3 fresnel(vec3 f0, float product) {
  product = clamp(product, 0.0, 1.0);

  return f0 + (1.0 - f0) * pow(1.0 - product, 5.0);
}

float G_neumann(float nl, float nv) {
  return nl * nv / max(nl, nv);
}

vec3 cookTorrance (float nl, float nv, float nh, float vh, vec3 f0, float roughness) {
  float D = D_GGX(roughness, nh);
  float G = G_neumann(nl, nv);

  return vec3(f0 * D * G);
}

void main() {
  vec2 fragCoord = viewUV * vec2(pc.resolution);
  bool hit;
  float cameraDistance = 15.0;
  vec3 eye = vec3(0.0, 0.0, cameraDistance);

  float worldScreenWidth = 4.0;
  float screenOffsetFromCamera = 5.0;
  vec3 worldPos = vec3((fragCoord - pc.resolution.xy * 0.5) / pc.resolution.x * worldScreenWidth, cameraDistance - screenOffsetFromCamera);

  vec3 dir = normalize(worldPos - eye);
  vec3 light = vec3(4.0, 4.0, 5.0);

  vec3 p = trace(eye, dir, hit);

  mat3 rotation = rotateEuler(vec3(0.0, radians(pc.time * 20.0), 0.0));

  if (hit) {
    vec3 n = generateNormal(p, 0.001);
    vec3 v = -dir;
    vec3 l = normalize(light - p);
    vec3 h = normalize(l + v);

    float imageScale = 3.5;
    vec2 imageOffset = vec2(0.5, 0.5);

    vec3 tp = rotation * p;
    vec3 tx = getTriplanarWeights(generateNormal(tp, 0.001));
    vec3 uvw = tp / imageScale;

    vec3 cx = texture(sideTexture, uvw.zy + imageOffset).xyz;
    vec3 cy = texture(sideTexture, uvw.zx).xyz;
    vec3 cz = texture(centerTexture, uvw.xy + imageOffset).xyz;

    vec3 textureValue = tx.x * cx + tx.z * cz;
    //vec3 base = vec3(1.0, 0.0, 0.0);
    vec3 base = vec3(1.0, 0.0, 0.0) * textureValue + vec3(0.3, 0.0, 0.0);
    base = pow(base, vec3(gamma));

    float roughness = 0.9;
    float metallness = 0.01;

    roughness = textureValue.x;
    metallness = 1.0 - textureValue.x;

    float nv = max(0.0, dot (n, v));
    float nl = max(0.0, dot (n, l));
    float nh = max(0.0, dot (n, h));
    float hl = max(0.0, dot (h, l));
    float vh = max(0.0, dot (h, v));

    vec3 F0 = mix(vec3(FDiel), base, metallness);
    vec3 specFresnel = fresnel(F0, vh);
    vec3 spec = cookTorrance(nl, nv, nh, vh, specFresnel, roughness) * nl / max (0.001, 4.0 * nl * nv);
    vec3 diff = (vec3(1.0) - specFresnel) * nl / pi;
    vec3 lightColor = vec3(1.0);

    vec4 color = vec4((diff * mix(base, vec3(0.0), metallness) + spec) * lightColor, 1.0);
    //vec4 color = vec4(spec, 1.0);
    vec4 gammaColor = pow(color, vec4(1.0 / gamma));
    outColor = gammaColor;
    //outColor = vec4(specFresnel, 1.0);
    //outColor = vec4(spec, 1.0);
  } else {
    outColor = vec4(0.0, 0.0, 0.0, 1.0);
  }

  vec2 uv = fragCoord / pc.resolution.xy;
  vec4 data = texture(sideTexture, uv);
//  outColor = vec4(data.rgb, 1.0);
}