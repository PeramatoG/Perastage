shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_prepass_alpha, shadows_disabled;

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
instance uniform float volumetric_noise_scale = 14.0;
uniform sampler2D gobo_texture : source_color, filter_linear_mipmap, repeat_disable;
instance uniform bool has_gobo = false;
instance uniform float gobo_top_radius = 0.02;
instance uniform float gobo_bottom_radius = 0.8;
instance uniform float gobo_mix : hint_range(0.0, 1.0) = 1.0;
instance uniform float gobo_floor : hint_range(0.0, 1.0) = 0.0;
instance uniform float gobo_contrast : hint_range(0.2, 3.0) = 1.3;

varying float beam_axial;
varying vec3 beam_local;

void vertex() {
	float normalized_y = (VERTEX.y / max(cone_height, 0.0001)) + 0.5;
	beam_axial = clamp(normalized_y, 0.0, 1.0);
	beam_local = VERTEX;
}

void fragment() {
	float near_factor = beam_axial;
	float far_progress = 1.0 - beam_axial;
	float end_fade = 1.0 - smoothstep(clamp(fade_end_ratio, 0.0, 0.99), 1.0, far_progress);
	float view_alignment = abs(dot(normalize(NORMAL), normalize(VIEW)));
	float lateral_fade = smoothstep(0.0, clamp(lateral_softness, 0.02, 1.0), view_alignment);
	float axial_noise = sin((beam_axial * volumetric_noise_scale) + (TIME * 0.9));
	float beam_noise = 1.0 + (axial_noise * volumetric_noise_strength);
	float center_boost = (1.0 + (lateral_emission_boost * lateral_fade)) * beam_noise;
	float gobo_mask = 1.0;
	if (has_gobo) {
		float local_radius = mix(gobo_bottom_radius, gobo_top_radius, beam_axial);
		local_radius = max(local_radius, 0.0001);
		vec2 beam_uv = vec2(beam_local.x, beam_local.z) / local_radius;
		beam_uv = (beam_uv * 0.5) + vec2(0.5);
		float raw_mask = 0.0;
		if (beam_uv.x < 0.0 || beam_uv.x > 1.0 || beam_uv.y < 0.0 || beam_uv.y > 1.0) {
			raw_mask = 0.0;
		} else {
			vec3 gobo_rgb = texture(gobo_texture, beam_uv).rgb;
			raw_mask = clamp(dot(gobo_rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
		}
		raw_mask = pow(raw_mask, gobo_contrast);
		float softened_mask = mix(gobo_floor, 1.0, raw_mask);
		gobo_mask = mix(1.0, softened_mask, gobo_mix);
	}
	ALBEDO = beam_color.rgb * gobo_mask;
	ALPHA = mix(far_alpha, near_alpha, near_factor) * end_fade * lateral_fade * gobo_mask;
	EMISSION = beam_color.rgb * (mix(far_emission, near_emission, near_factor) * end_fade * center_boost * gobo_mask);
}
