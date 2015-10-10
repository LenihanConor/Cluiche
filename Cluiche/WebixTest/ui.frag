uniform sampler2D uiOverlayTex;
uniform sampler2D backBufferTex;

void main(void)
{
    vec4 backbufferTexture = texture2D(backBufferTex, gl_TexCoord[0].st);
    vec4 uiTexture = texture2D(uiOverlayTex, gl_TexCoord[0].st);
    
	gl_FragColor = backbufferTexture * (1.0 - uiTexture.w) + (uiTexture * uiTexture.w);
	//gl_FragColor = uiTexture;
}