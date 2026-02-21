shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_prepass_alpha, shadows_disabled;

uniform bool gobo_enabled = false;
uniform sampler2D gobo_texture : source_color, filter_linear_mipmap, repeat_disable;
uniform float gobo_strength : hint_range(0.0, 1.0) = 1.0;
uniform float gobo_rotation_rad : hint_range(-6.283, 6.283) = 0.0;
uniform float gobo_mip_bias : hint_range(0.0, 6.0) = 0.0;
uniform float gobo_gamma : hint_range(0.25, 4.0) = 1.0;

instance uniform vec4 beam_color : source_color = vec4(1.0, 0.85, 0.55, 1.0);
instance uniform float near_alpha = 0.16;
instance uniform float far_alpha = 0.004;
instance uniform float near_emission = 0.45;
instance uniform float far_emission = 0.04;
instance uniform float cone_height = 15.0;
instance uniform float cone_top_radius = 0.02;
instance uniform float cone_bottom_radius = 0.8;
instance uniform float fade_end_ratio = 0.82;
instance uniform float lateral_softness = 0.35;
instance uniform float lateral_emission_boost = 0.16;
instance uniform float volumetric_noise_strength = 0.06;
instance uniform float volumetric_noise_scale = 14.0;

varying float beam_axial;
varying vec3 local_pos;

float sample_gobo_beamspace() {
	float axial01 = clamp((local_pos.y / max(cone_height, 0.0001)) + 0.5, 0.0, 1.0);
	float slice_radius = mix(cone_top_radius, cone_bottom_radius, axial01);
	if (slice_radius <= 0.0001) {
		return 1.0;
	}

	vec2 plane = local_pos.xz / slice_radius;
	float c = cos(gobo_rotation_rad);
	float s = sin(gobo_rotation_rad);
	mat2 rot = mat2(c, -s, s, c);
	vec2 uv = (rot * plane) * 0.5 + vec2(0.5);
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
		return 0.0;
	}
	float raw = textureLod(gobo_texture, uv, gobo_mip_bias).r;
	float curved = pow(max(raw, 0.0001), gobo_gamma);
	return mix(1.0, curved, gobo_strength);
}

void vertex() {
	float normalized_y = (VERTEX.y / max(cone_height, 0.0001)) + 0.5;
	beam_axial = clamp(normalized_y, 0.0, 1.0);
	local_pos = VERTEX;
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
	float gobo_multiplier = 1.0;
	if (gobo_enabled) {
		gobo_multiplier = sample_gobo_beamspace();
		float edge_through = mix(0.75, 1.0, lateral_fade);
		gobo_multiplier *= edge_through;
	}
	ALBEDO = beam_color.rgb;
	ALPHA = mix(far_alpha, near_alpha, near_factor) * end_fade * lateral_fade * gobo_multiplier;
	EMISSION = beam_color.rgb * (mix(far_emission, near_emission, near_factor) * end_fade * center_boost * gobo_multiplier);
}
