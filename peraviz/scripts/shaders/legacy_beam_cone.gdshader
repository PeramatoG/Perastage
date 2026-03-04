shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_prepass_alpha, shadows_disabled;

uniform sampler2D gobo_texture : source_color, hint_default_white;

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

instance uniform bool gobo_enabled = false;
instance uniform float gobo_cutoff = 0.5;
instance uniform float gobo_softness = 0.04;
instance uniform float gobo_start_ratio = 0.01;
instance uniform float gobo_rotation = 0.0;
instance uniform vec2 gobo_size = vec2(0.08, 0.08);

varying float beam_axial;
varying vec3 v_local;

float sample_gobo_open(vec2 gobo_uv) {
	vec4 texel = texture(gobo_texture, gobo_uv);
	float lum = dot(texel.rgb, vec3(0.299, 0.587, 0.114)) * texel.a;

	if (gobo_softness <= 0.00001) {
		return step(gobo_cutoff, lum);
	}
	return smoothstep(gobo_cutoff - gobo_softness, gobo_cutoff + gobo_softness, lum);
}

void vertex() {
	float normalized_y = (VERTEX.y / max(cone_height, 0.0001)) + 0.5;
	beam_axial = clamp(normalized_y, 0.0, 1.0);
	v_local = VERTEX;
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

	float gobo_open = 1.0;
	if (gobo_enabled) {
		float axis_pos = (cone_height * 0.5) - v_local.y;
		float axis_dist = clamp(axis_pos, 0.001, cone_height);
		float gobo_plane_dist = max(gobo_start_ratio * cone_height, 0.001);

		if (axis_pos >= gobo_plane_dist) {
			vec2 radial_pos = vec2(v_local.x, -v_local.z);
			float projection_scale = gobo_plane_dist / axis_dist;
			vec2 gobo_plane_pos = radial_pos * projection_scale;

			float sin_rot = sin(gobo_rotation);
			float cos_rot = cos(gobo_rotation);
			vec2 rotated = vec2(
				gobo_plane_pos.x * cos_rot - gobo_plane_pos.y * sin_rot,
				gobo_plane_pos.x * sin_rot + gobo_plane_pos.y * cos_rot
			);

			vec2 half_size = max(gobo_size * 0.5, vec2(0.001));
			vec2 gobo_uv = rotated / half_size + vec2(0.5);

			if (gobo_uv.x < 0.0 || gobo_uv.x > 1.0 || gobo_uv.y < 0.0 || gobo_uv.y > 1.0) {
				gobo_open = 0.0;
			} else {
				gobo_open = sample_gobo_open(gobo_uv);
			}
		}
	}

	ALBEDO = beam_color.rgb;
	ALPHA = mix(far_alpha, near_alpha, near_factor) * end_fade * lateral_fade * gobo_open;
	EMISSION = beam_color.rgb * (mix(far_emission, near_emission, near_factor) * end_fade * center_boost * gobo_open);
}
