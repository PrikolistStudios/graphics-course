#version 450

layout(location = 0) out vec2 outUV;

void main() {
  // Generates UVs: (0, 0), (2, 0), (0, 2)
  outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);

  // Maps UVs to Clip Space / NDC: [-1, 1] for the screen, extending to 3
  //  gl_Position = vec4(outUV.x * 2.0f - 1.0f, outUV.y, 0.0f, 1.0f);
  gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
  gl_Position.y *= -1.0f;
}
