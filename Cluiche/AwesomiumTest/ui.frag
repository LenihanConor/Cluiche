uniform sampler2D uiOverlayTex;
uniform sampler2D backBufferTex;

void main(void)
{
    vec4 t0 = texture2D(backBufferTex, gl_TexCoord[0].xy);
    vec4 t1 = texture2D(uiOverlayTex, gl_TexCoord[0].xy);
    
	gl_FragColor = t0 * (1.0 - t1.w) + (t1 * t1.w);

	//gl_FragColor = vec4(t1.w, 0.0, 0.0, 1.0);
}