# Pipeline Stages

Vulkan exposes the following pipeline stages:

1. TOP_OF_PIPE_BIT
2. DRAW_INDIRECT_BIT
3. VERTEX_INPUT_BIT
4. VERTEX_SHADER_BIT
5. TESSELLATION_CONTROL_SHADER_BIT
6. TESSELLATION_EVALUATION_SHADER_BIT
7. GEOMETRY_SHADER_BIT
8. FRAGMENT_SHADER_BIT
9. EARLY_FRAGMENT_TESTS_BIT
10. LATE_FRAGMENT_TESTS_BIT
11. COLOR_ATTACHMENT_OUTPUT_BIT
12. TRANSFER_BIT
13. COMPUTE_SHADER_BIT
14. BOTTOM_OF_PIPE_BIT

There are also the following pseudo-stages which combine multiple stages or handle special access:

1. HOST_BIT
2. ALL_GRAPHICS_BIT
3. ALL_COMMANDS_BIT