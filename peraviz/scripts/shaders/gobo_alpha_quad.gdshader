shader_type spatial;
render_mode blend_mix, depth_prepass_alpha, cull_disabled, unshaded;

uniform sampler2D gobo_texture : source_color;
uniform float alpha_cutoff : hint_range(0.0, 1.0) = 0.35;

void fragment() {
	vec4 gobo_sample = texture(gobo_texture, UV);
	float luma = dot(gobo_sample.rgb, vec3(0.299, 0.587, 0.114));
	float blocker = clamp((1.0 - luma) * gobo_sample.a, 0.0, 1.0);
	ALBEDO = vec3(0.0);
	ALPHA = blocker;
	ALPHA_SCISSOR_THRESHOLD = alpha_cutoff;
}
