//
// Created by ev-ka on 30.10.2026.
//

#ifndef GRAPHICS_COURSE_RENDERCONTEXT_HPP
#define GRAPHICS_COURSE_RENDERCONTEXT_HPP

struct RenderContext
{
  vk::CommandBuffer cmd_buf;
  vk::Image target_image;
  vk::ImageView target_image_view;
  glm::uvec2 swapchain_resolution;
  etna::Image& main_view_depth;
};

#endif // GRAPHICS_COURSE_RENDERCONTEXT_HPP
