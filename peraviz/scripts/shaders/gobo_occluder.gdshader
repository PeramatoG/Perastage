shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_lambert, specular_schlick_ggx;

uniform sampler2D gobo_texture;

void fragment() {
	vec4 n_out2p0;
	n_out2p0 = texture(gobo_texture, UV);

	float n_out3p0;
	float n_in3p1 = 0.0;
	float n_in3p2 = 1.0;
	float n_in3p3 = 1.0;
	float n_in3p4 = 0.0;
	{
		float __input_range = n_in3p2 - n_in3p1;
		float __output_range = n_in3p4 - n_in3p3;
		n_out3p0 = n_in3p3 + __output_range * ((n_out2p0.x - n_in3p1) / __input_range);
	}

	float n_out4p0 = 0.5;
	ALPHA = n_out3p0;
	ALPHA_SCISSOR_THRESHOLD = n_out4p0;
}
