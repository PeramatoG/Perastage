shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back;

uniform sampler2D gobo_texture : source_color, hint_default_white;

void fragment() {
	vec4 texel = texture(gobo_texture, UV);
	float luma = dot(texel.rgb, vec3(0.299, 0.587, 0.114)) * texel.a;
	float alpha_block = 1.0 - luma;
	ALPHA = alpha_block;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
