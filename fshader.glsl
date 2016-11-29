// CRT emulation filter fragment shader
precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D s_texture;
uniform float u_framecount;
uniform vec2 u_emulator_frame_size;
uniform vec2 u_output_frame_size;

//#define TEXSTEP_H (0.50/800.0)
//#define TEXSTEP_V (0.25/480.0)
//#define TEXSTEP_H (0.50/1200.0)
//#define TEXSTEP_V (0.25/720.0)

void main()
{
	float tsh = 0.5 / 1200.0;
	float tsv = 0.5 / 720.0;
	vec2 tc = v_texCoord + vec2(tsh, tsv + sin(u_framecount) * 0.0);
	// gl_FragColor = texture2D( s_texture, v_texCoord );
	vec4 pix5 = texture2D( s_texture, tc ) * 0.8; // main, center pixel

	vec4 pix2 = texture2D( s_texture, tc + vec2(0.0, -tsv) );
	vec4 pix4 = texture2D( s_texture, tc + vec2(-tsh, 0.0) );
	vec4 pix6 = texture2D( s_texture, tc + vec2(+tsh, 0.0) );
	vec4 pix8 = texture2D( s_texture, tc + vec2(0.0, +tsv) );

	gl_FragColor = 
		pix5
		//* clamp(cos(gl_FragCoord.y * 3.1415926) * 0.5 + 0.5, 0.0, 1.0)
		* clamp(cos(v_texCoord.y * 3.1415926 * 720.0) * 0.25 + 0.92, 0.0, 1.0)
		
		// * vec4((sin(u_framecount) * 0.005 + 0.98))
		+ (
			  pow(pix2 * .7, vec4(1.5))
			 + pow(pix4 * .7, vec4(1.5))
			 + pow(pix6 * .7, vec4(1.5))
			 + pow(pix8 * .7, vec4(1.5))

		  ) * 0.3
		; 
//	gl_FragColor = texture2D( s_texture, v_texCoord );

}

