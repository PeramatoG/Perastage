shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never;

uniform sampler2D DEPTH_TEXTURE : hint_depth_texture, filter_linear_mipmap;
// Godot Light3D projector textures affect lit surfaces, not built-in volumetric fog.
// This shader performs explicit gobo masking inside the beam volume.
// The gobo mask sampled in beam space. White passes light; black blocks it.
uniform sampler2D gobo_texture : source_color, hint_default_white;

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

// Whether to modulate the volumetric beam by the gobo pattern.
instance uniform bool gobo_enabled = false;
instance uniform float gobo_cutoff = 0.5;
instance uniform float gobo_start_ratio = 0.0025;
instance uniform float gobo_rotation = 0.0;
instance uniform vec2 gobo_size = vec2(0.08, 0.08);
instance uniform float gobo_axis_sign = 1.0;
instance uniform float beam_height = 8.0;
instance uniform float beam_top_radius = 0.02;
instance uniform float beam_bottom_radius = 0.8;

varying vec3 v_view;
varying vec3 v_local;

void vertex() {
	v_local = VERTEX;
	v_view = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
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
	float gobo_open = 1.0;
	if (gobo_enabled) {
		float axis_pos = (beam_height * 0.5) - (v_local.y * gobo_axis_sign);
		float axis_dist = clamp(axis_pos, 0.001, beam_height);
		float gobo_plane_dist = gobo_start_ratio * beam_height;
		if (axis_pos >= gobo_plane_dist) {
			float t = axis_pos / beam_height;
			float radius_at_point = mix(beam_top_radius, beam_bottom_radius, clamp(t, 0.0, 1.0));
			vec2 radial_pos = vec2(v_local.x, v_local.z);
			if (length(radial_pos) > (radius_at_point + 0.001)) {
				discard;
			}

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
				const int LINE_SAMPLE_COUNT = 2;
				float gobo_acc = 0.0;
				for (int i = 0; i < LINE_SAMPLE_COUNT; i++) {
					float sample_t = (float(i) + 0.5) / float(LINE_SAMPLE_COUNT);
					vec2 sample_uv = mix(vec2(0.5), gobo_uv, sample_t);
					vec4 texel = texture(gobo_texture, sample_uv);
					float luminance = dot(texel.rgb, vec3(0.299, 0.587, 0.114)) * texel.a;
					gobo_acc += smoothstep(gobo_cutoff - 0.04, gobo_cutoff + 0.04, luminance);
				}
				gobo_open = gobo_acc / float(LINE_SAMPLE_COUNT);
			}
		} else {
			gobo_open = 1.0;
		}
	}

	brightness *= gobo_open;
	final_alpha *= gobo_open;
	if (final_alpha < 0.001) {
		discard;
	}

	ALBEDO = base_color.rgb * brightness;
	ALPHA = clamp(final_alpha * base_color.a, 0.0, 1.0);
	EMISSION = ALBEDO * ALPHA;
}
