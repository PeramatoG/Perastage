shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_disabled, unshaded;

// Cookie texture used as a shadow-casting occluder near the lens.
uniform sampler2D gobo_texture : source_color, filter_linear, repeat_disable;
// Invert the default mask interpretation when a fixture/media requires it.
uniform bool invert_mask = false;
// Allows matching wheel orientation against fixture conventions.
uniform float rotation_rad = 0.0;
// Threshold for alpha scissor so the quad casts binary shadows in volumetric fog.
uniform float alpha_scissor_threshold : hint_range(0.01, 0.99) = 0.5;

void fragment() {
	vec2 centered_uv = UV - vec2(0.5);
	float sin_rot = sin(rotation_rad);
	float cos_rot = cos(rotation_rad);
	vec2 rotated_uv = vec2(
		centered_uv.x * cos_rot - centered_uv.y * sin_rot,
		centered_uv.x * sin_rot + centered_uv.y * cos_rot
	) + vec2(0.5);

	vec4 texel = texture(gobo_texture, rotated_uv);
	// Gobo media in Peraviz is authored as black = blocked, white = transmitted.
	// Convert to occluder alpha (white blocks) so footprint and volumetric shadow match.
	float transmittance = clamp(texel.r * texel.a, 0.0, 1.0);
	float mask = 1.0 - transmittance;
	if (invert_mask) {
		mask = 1.0 - mask;
	}
	ALPHA = clamp(mask, 0.0, 1.0);
	ALPHA_SCISSOR_THRESHOLD = alpha_scissor_threshold;
}
