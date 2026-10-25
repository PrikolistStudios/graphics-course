
add_executable(terrain
        main.cpp
        Renderer.cpp
        WorldRenderer.cpp
        App.cpp
        PerlinNoiseGenerator.cpp
)

target_link_libraries(terrain
        PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(terrain
        shaders/perlin.comp.glsl
        shaders/simple_shadow.frag.glsl
        shaders/simple.vert.glsl
)
