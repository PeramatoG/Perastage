shader_type spatial;
render_mode blend_mix, depth_prepass_alpha, cull_disabled, diffuse_lambert, specular_schlick_ggx;

uniform sampler2D gobo_texture;
uniform float gobo_rotation_deg : hint_range(-360.0, 360.0) = 0.0;
uniform float gobo_scale : hint_range(0.05, 8.0) = 1.0;

vec2 rotate2d(vec2 value, float angle_rad) {
	float c = cos(angle_rad);
	float s = sin(angle_rad);
	return vec2((c * value.x) - (s * value.y), (s * value.x) + (c * value.y));
}

void fragment() {
	vec2 centered_uv = UV - vec2(0.5);
	vec2 rotated_uv = rotate2d(centered_uv / max(gobo_scale, 0.05), radians(gobo_rotation_deg));
	vec2 projected_uv = rotated_uv + vec2(0.5);
	vec4 gobo_sample = texture(gobo_texture, projected_uv);
	float alpha_mask = mix(1.0, 0.0, clamp(gobo_sample.r, 0.0, 1.0));
	ALBEDO = vec3(1.0);
	ALPHA = alpha_mask;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
