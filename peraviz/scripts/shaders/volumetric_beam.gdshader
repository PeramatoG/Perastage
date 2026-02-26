shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never;

uniform sampler2D DEPTH_TEXTURE : hint_depth_texture, filter_linear_mipmap;
# Godot Light3D projector textures affect lit surfaces, not built-in volumetric fog.
# This shader performs explicit gobo masking inside the beam volume.
# The gobo mask sampled in beam space. White passes light; black blocks it.
uniform sampler2D gobo_texture : hint_black_white;

instance uniform vec4 base_color : source_color = vec4(1.0, 0.95, 0.85, 0.5);
uniform float falloff_power : hint_range(0.1, 10.0) = 8.0;
uniform float facing_boost : hint_range(0.0, 10.0) = 1.5;
uniform float facing_power : hint_range(0.1, 20.0) = 4.0;
uniform float boost_along_y : hint_range(0.0, 10.0) = 1.0;
uniform float feather_sharpness : hint_range(0.1, 20.0) = 4.0;
uniform float feather_intensity : hint_range(0.0, 1.0) = 1.0;
uniform float near_fade_start : hint_range(0.0, 50.0) = 0.01;
uniform float near_fade_end : hint_range(0.0, 50.0) = 1.0;
uniform float far_fade_start : hint_range(0.1, 100.0) = 25.0;
uniform float far_fade_end : hint_range(0.1, 100.0) = 80.0;
instance uniform float max_brightness : hint_range(1.0, 50.0) = 10.0;
uniform bool depth_feather_enabled = true;
uniform float depth_fade_distance : hint_range(0.01, 5.0) = 0.5;

# Whether to modulate the volumetric beam by the gobo pattern.
instance uniform bool gobo_enabled = false;
# Ratio of beam radius to distance from the emitter gate: tan(spot_angle / 2).
instance uniform float tan_half_angle = 0.0;
# How many radial samples to take while integrating the mask from center to shell.
instance uniform int radial_samples = 4;

varying vec3 v_view;
varying float v_depth;

void vertex() {
	v_view = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	# Distance from the fixture along the beam axis (positive along -Z in view space).
	v_depth = -v_view.z;
	POSITION = PROJECTION_MATRIX * vec4(v_view, 1.0);
}

float get_linear_depth(float raw_depth, mat4 inv_proj_mat) {
	return 1.0 / (raw_depth * inv_proj_mat[2][3] + inv_proj_mat[3][3]);
}

void fragment() {
	float dist = length(v_view);
	float near_alpha = smoothstep(near_fade_start, near_fade_end, dist);
	float far_alpha = 1.0 - smoothstep(far_fade_start, far_fade_end, dist);
	float dist_fade = near_alpha * far_alpha;
	if (dist_fade < 0.001) {
		discard;
	}

	float apex_fade = pow(max(1.0 - UV.y, 0.001), falloff_power);
	float base_brightness = base_color.a * apex_fade;

	vec3 n = normalize(NORMAL);
	vec3 v = normalize(-v_view);
	float ndotv = max(0.0, dot(n, v));
	float location_weight = pow(max(1.0 - UV.y, 0.0), boost_along_y);
	float facing_multiplier = 1.0 + (facing_boost * location_weight * pow(ndotv, facing_power));

	float brightness = min(base_brightness * facing_multiplier, max_brightness);
	float feather_alpha = mix(1.0 - feather_intensity, 1.0, pow(ndotv, feather_sharpness));

	float depth_mult = 1.0;
	if (depth_feather_enabled) {
		float scene_depth = texture(DEPTH_TEXTURE, SCREEN_UV).r;
		if (scene_depth < 0.9999) {
			float scene_linear = get_linear_depth(scene_depth, INV_PROJECTION_MATRIX);
			float frag_linear = -v_view.z;
			float diff = scene_linear - frag_linear;
			depth_mult = smoothstep(0.0, depth_fade_distance, diff);
		}
	}

	float final_alpha = feather_alpha * depth_mult * dist_fade;
	float gobo_intensity = 1.0;
	if (gobo_enabled) {
		if (v_depth <= 0.0 || tan_half_angle <= 0.0) {
			discard;
		}

		# Project the fragment onto the gobo gate at the emitter and let UVs expand with distance.
		vec2 gobo_uv = vec2(0.5) + (v_view.xy / (v_depth * 2.0 * tan_half_angle));
		const int MAX_RADIAL_SAMPLES = 16;
		int sample_count = clamp(radial_samples, 1, MAX_RADIAL_SAMPLES);
		float gobo_acc = 0.0;
		# Integrate from center to shell so blocked areas attenuate the entire beam interior,
		# mirroring a physical gobo plate with shaped holes.
		for (int i = 0; i < MAX_RADIAL_SAMPLES; i++) {
			if (i >= sample_count) {
				break;
			}
			float t = (float(i) + 0.5) / float(sample_count);
			vec2 sample_uv = mix(vec2(0.5, 0.5), gobo_uv, t);
			gobo_acc += texture(gobo_texture, sample_uv).r;
		}
		gobo_intensity = gobo_acc / float(sample_count);
		if (gobo_intensity < 0.001) {
			discard;
		}
	}

	brightness *= gobo_intensity;
	final_alpha *= gobo_intensity;
	if (final_alpha < 0.001) {
		discard;
	}

	ALBEDO = base_color.rgb * brightness;
	ALPHA = clamp(final_alpha * base_color.a, 0.0, 1.0);
	EMISSION = ALBEDO * ALPHA;
}
