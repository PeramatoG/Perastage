shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_disabled, unshaded;

uniform sampler2D gobo_texture : source_color, filter_linear_mipmap, repeat_disable;

// Shadow-cookie path: alpha scissor on this quad drives the SpotLight3D shadow map.
// Keep this material on an occluder mesh near the lens to get volumetric fog patterning.
void fragment() {
	vec4 n_out2p0;
// Texture2D:2
	n_out2p0 = texture(gobo_texture, UV);


	float n_out3p0;
// Remap:3
	float n_in3p1 = 0.00000;
	float n_in3p2 = 1.00000;
	float n_in3p3 = 1.00000;
	float n_in3p4 = 0.00000;
	{
		float __input_range = n_in3p2 - n_in3p1;
		float __output_range = n_in3p4 - n_in3p3;
		n_out3p0 = n_in3p3 + __output_range * ((n_out2p0.x - n_in3p1) / __input_range);
	}


// FloatConstant:4
	float n_out4p0 = 0.500000;


// Output:0
	ALPHA = n_out3p0;
	ALPHA_SCISSOR_THRESHOLD = n_out4p0;


}
