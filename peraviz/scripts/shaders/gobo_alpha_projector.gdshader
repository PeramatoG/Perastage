shader_type spatial;
render_mode blend_mix, depth_prepass_alpha, cull_disabled, diffuse_lambert, specular_schlick_ggx;

uniform sampler2D gobo_texture;
uniform sampler2D gobo_texture_secondary;
uniform bool gobo_transition_enabled = false;
uniform float gobo_transition_progress = 0.0;
uniform float gobo_transition_direction = 1.0;

vec4 sample_masked_texture(sampler2D tex, vec2 uv) {
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
		return vec4(0.0, 0.0, 0.0, 1.0);
	}
	return texture(tex, uv);
}

void fragment() {
	vec4 primary_sample = texture(gobo_texture, UV);
	float primary_luma = clamp(primary_sample.r, 0.0, 1.0);
	float final_luma = primary_luma;

	if (gobo_transition_enabled) {
		float progress = clamp(gobo_transition_progress, 0.0, 1.0);
		float dir = gobo_transition_direction >= 0.0 ? 1.0 : -1.0;
		vec2 uv_primary = UV;
		vec2 uv_secondary = UV;
		uv_primary.x -= progress * dir;
		uv_secondary.x -= (progress - 1.0) * dir;
		vec4 shifted_primary = sample_masked_texture(gobo_texture, uv_primary);
		vec4 shifted_secondary = sample_masked_texture(gobo_texture_secondary, uv_secondary);
		float shifted_primary_luma = clamp(shifted_primary.r, 0.0, 1.0);
		float shifted_secondary_luma = clamp(shifted_secondary.r, 0.0, 1.0);
		final_luma = max(shifted_primary_luma, shifted_secondary_luma);
	}

	float alpha_mask = mix(1.0, 0.0, final_luma);
	ALBEDO = vec3(1.0);
	ALPHA = alpha_mask;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
