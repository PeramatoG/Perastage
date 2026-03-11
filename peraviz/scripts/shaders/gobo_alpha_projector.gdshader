shader_type spatial;
render_mode blend_mix, depth_prepass_alpha, cull_disabled, diffuse_lambert, specular_schlick_ggx;

uniform sampler2D gobo_texture;

void fragment() {
	vec4 gobo_sample = texture(gobo_texture, UV);
	float alpha_mask = mix(1.0, 0.0, clamp(gobo_sample.r, 0.0, 1.0));
	ALBEDO = vec3(1.0);
	ALPHA = alpha_mask;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
