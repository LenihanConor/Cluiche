$input v_worldPos

#include <bgfx_shader.sh>

void main()
{
    // Depth-only pass — no colour output needed.
    // bgfx writes depth automatically; fragment shader is a no-op.
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
