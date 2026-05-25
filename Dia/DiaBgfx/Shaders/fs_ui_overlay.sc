$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_uiOverlay, 0);

void main()
{
    vec4 ui = texture2D(s_uiOverlay, v_texcoord0.xy);
    gl_FragColor = ui;
}
