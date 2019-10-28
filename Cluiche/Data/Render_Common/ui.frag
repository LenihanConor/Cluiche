
uniform sampler2D backBufferTex;
uniform sampler2D uiOverlayTex;

void main(void)
{
    vec4 backbufferTexture = texture2D(backBufferTex, gl_TexCoord[0].xy);
    vec4 uiTexture = texture2D(uiOverlayTex, gl_TexCoord[0].xy);

	   gl_FragColor = backbufferTexture * (1.0 - uiTexture.w) + (uiTexture * uiTexture.w);
}
