shader_type spatial;
render_mode blend_mix, depth_draw_alpha_prepass, cull_disabled, unshaded;

uniform sampler2D gobo_texture : source_color;
uniform float alpha_cutoff : hint_range(0.0, 1.0) = 0.5;

void fragment() {
	vec4 gobo_sample = texture(gobo_texture, UV);
	float luma = dot(gobo_sample.rgb, vec3(0.299, 0.587, 0.114));
	float transmission = clamp(luma * gobo_sample.a, 0.0, 1.0);
	ALBEDO = vec3(0.0);
	ALPHA = 1.0 - transmission;
	ALPHA_SCISSOR_THRESHOLD = alpha_cutoff;
}
