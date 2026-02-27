shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_lambert, specular_schlick_ggx;

uniform sampler2D gobo_texture;

void fragment() {
	vec4 sampled_texture = texture(gobo_texture, UV);
	float gobo_alpha = 1.0 - sampled_texture.r;
	ALPHA = gobo_alpha;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
