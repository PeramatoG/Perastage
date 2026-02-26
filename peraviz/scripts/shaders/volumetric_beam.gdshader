shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_opaque;

uniform sampler2D DEPTH_TEXTURE : hint_depth_texture, filter_linear_mipmap;
uniform sampler2D gobo_texture;

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
instance uniform bool gobo_enabled = false;
instance uniform float gobo_cutoff : hint_range(0.0, 1.0) = 0.5;
instance uniform float gobo_start_ratio : hint_range(0.0, 1.0) = 0.0;
instance uniform float gobo_rotation : hint_range(-6.2831853, 6.2831853) = 0.0;
instance uniform vec2 gobo_size = vec2(0.08, 0.08);

varying vec3 vertex_view;
varying vec3 vertex_local;

void vertex() {
	vertex_local = VERTEX;
	vertex_view = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	POSITION = PROJECTION_MATRIX * vec4(vertex_view, 1.0);
}

float get_linear_depth(float raw_depth, mat4 inv_proj_mat) {
	return 1.0 / (raw_depth * inv_proj_mat[2][3] + inv_proj_mat[3][3]);
}

void fragment() {
	float dist = length(vertex_view);
	float near_alpha = smoothstep(near_fade_start, near_fade_end, dist);
	float far_alpha = 1.0 - smoothstep(far_fade_start, far_fade_end, dist);
	float dist_fade = near_alpha * far_alpha;
	if (dist_fade < 0.001) {
		discard;
	}

	float apex_fade = pow(max(1.0 - UV.y, 0.001), falloff_power);
	float base_brightness = base_color.a * apex_fade;

	vec3 n = normalize(NORMAL);
	vec3 v = normalize(-vertex_view);
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
			float frag_linear = -vertex_view.z;
			float diff = scene_linear - frag_linear;
			depth_mult = smoothstep(0.0, depth_fade_distance, diff);
		}
	}

	float final_alpha = feather_alpha * depth_mult * dist_fade;
	if (gobo_enabled && UV.y >= gobo_start_ratio) {
		float depth_ratio = max(UV.y, 0.001);
		float projection_scale = gobo_start_ratio / depth_ratio;
		vec2 gobo_plane_pos = vertex_local.xz * projection_scale;
		float sin_angle = sin(gobo_rotation);
		float cos_angle = cos(gobo_rotation);
		vec2 rotated = vec2(
			gobo_plane_pos.x * cos_angle - gobo_plane_pos.y * sin_angle,
			gobo_plane_pos.x * sin_angle + gobo_plane_pos.y * cos_angle
		);
		vec2 half_size = max(gobo_size * 0.5, vec2(0.001));
		vec2 gobo_uv = rotated / half_size * 0.5 + 0.5;
		float inside = step(0.0, gobo_uv.x) * step(gobo_uv.x, 1.0) * step(0.0, gobo_uv.y) * step(gobo_uv.y, 1.0);
		float gobo_open = 0.0;
		if (inside > 0.5) {
			vec4 texel = texture(gobo_texture, gobo_uv);
			float luma = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
			float effective_luma = luma * texel.a;
			gobo_open = step(gobo_cutoff, effective_luma);
		}
		brightness *= gobo_open;
		final_alpha *= gobo_open;
	}

	ALBEDO = base_color.rgb * brightness;
	ALPHA = clamp(final_alpha * base_color.a, 0.0, 1.0);
	EMISSION = ALBEDO * ALPHA;
}
