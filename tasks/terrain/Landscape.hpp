//
// Created by ev-ka on 26.10.2026.
//

#ifndef GRAPHICS_COURSE_LANDSCAPERENDERER_HPP
#define GRAPHICS_COURSE_LANDSCAPERENDERER_HPP

#include "PerlinNoiseGenerator.hpp"
#include "shaders/LandscapeShaderParams.h"
#include "etna/BlockingTransferHelper.hpp"
#include "RenderContext.hpp"

#include "render_utils/QuadRenderer.hpp"

class Landscape
{
public:
  struct Params
  {
    glm::uint cell_num = 512;
    glm::float32 size = 10.f;
    glm::float32 height = 2.5f;
  };

  explicit Landscape(const Params&);
  void init(vk::Format swapchain_format);
  void render(const RenderContext& ctx);
  void drawGui();

private:
  struct Vertex
  {
    glm::vec3 position;
  };

  enum TextureType
  {
    BaseColor,
    Roughness,
    HeightMap,
    NormalMap,
  };

  struct BasicPBR
  {
    etna::Image base_color;
    etna::Image roughness;
    etna::Image heightmap;
  };

  void regenerateIBuf();
  void updateVertShader();
  void loadTextures();
  etna::Image loadTexture(const std::string_view& path, TextureType textureType);

  std::unique_ptr<etna::OneShotCmdMgr> one_shot_commands;
  etna::BlockingTransferHelper transfer_helper;

  Params params;
  int newCellNum;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  BasicPBR grass;
  BasicPBR rock;
  BasicPBR sand;
  BasicPBR snow;
  std::vector<etna::Binding> textures_bindings;

  PerlinNoiseGenerator::Params noise_params;
  PerlinNoiseGenerator noise_gen;
  bool drawNoiseTex = false;

  etna::GraphicsPipeline pipeline;
  std::unique_ptr<QuadRenderer> quadRenderer;
  etna::Sampler defaultSampler;

  etna::Image noise_tx;
  etna::Buffer index_buffer;
  etna::Buffer old_ind_buf;
  etna::Buffer vert_shader_params_buf;
};


#endif // GRAPHICS_COURSE_LANDSCAPERENDERER_HPP
