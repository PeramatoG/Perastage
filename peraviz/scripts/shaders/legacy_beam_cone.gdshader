shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never, depth_test_disabled;

instance uniform vec4 beam_color : source_color = vec4(1.0, 0.85, 0.55, 1.0);
instance uniform float near_alpha = 0.16;
instance uniform float far_alpha = 0.004;
instance uniform float near_emission = 0.45;
instance uniform float far_emission = 0.04;
instance uniform float cone_height = 15.0;
instance uniform float fade_end_ratio = 0.82;
instance uniform float lateral_softness = 0.35;
instance uniform float lateral_emission_boost = 0.16;
instance uniform float volumetric_noise_strength = 0.06;

varying float beam_axial;
varying float radial_ratio;

void vertex() {
	float normalized_y = (VERTEX.y / max(cone_height, 0.001)) + 0.5;
	beam_axial = clamp(normalized_y, 0.0, 1.0);
	radial_ratio = clamp(UV.x, 0.0, 1.0);
}

void fragment() {
	float near_factor = beam_axial;
	float far_progress = 1.0 - beam_axial;
	float end_fade = 1.0 - smoothstep(fade_end_ratio, 1.0, beam_axial);
	float lateral_fade = 1.0 - smoothstep(1.0 - lateral_softness, 1.0, radial_ratio);
	float axial_noise = sin((beam_axial * 14.0) + (TIME * 0.9));
	float beam_noise = 1.0 + (axial_noise * volumetric_noise_strength);
	float center_boost = (1.0 + (lateral_emission_boost * lateral_fade)) * beam_noise;
	ALBEDO = beam_color.rgb;
	ALPHA = mix(far_alpha, near_alpha, near_factor) * end_fade * lateral_fade;
	EMISSION = beam_color.rgb * (mix(far_emission, near_emission, near_factor) * end_fade * center_boost);
}
