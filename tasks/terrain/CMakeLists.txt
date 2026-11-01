
add_executable(terrain
        main.cpp
        Renderer.cpp
        WorldRenderer.cpp
        App.cpp
        PerlinNoiseGenerator.cpp
        Landscape.cpp
)

target_link_libraries(terrain
        PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(terrain
        shaders/perlin.comp.glsl
        shaders/terrain.frag.glsl
        shaders/terrain.vert.glsl
        shaders/base.vert.glsl
        shaders/base.frag.glsl
)
