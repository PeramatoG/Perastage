shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_opaque;

uniform sampler2D DEPTH_TEXTURE : hint_depth_texture, filter_linear_mipmap;
uniform bool gobo_enabled = false;
uniform sampler2D gobo_texture : source_color, filter_linear_mipmap, repeat_disable;
uniform float gobo_strength : hint_range(0.0, 1.0) = 1.0;
uniform float gobo_rotation_rad : hint_range(-6.283, 6.283) = 0.0;
uniform float gobo_mip_bias : hint_range(0.0, 6.0) = 0.0;
uniform float gobo_gamma : hint_range(0.25, 4.0) = 1.0;

instance uniform vec4 base_color : source_color = vec4(1.0, 0.95, 0.85, 0.5);
instance uniform float falloff_power : hint_range(0.1, 10.0) = 8.0;
instance uniform float facing_boost : hint_range(0.0, 10.0) = 1.5;
instance uniform float facing_power : hint_range(0.1, 20.0) = 4.0;
instance uniform float boost_along_y : hint_range(0.0, 10.0) = 1.0;
instance uniform float feather_sharpness : hint_range(0.1, 20.0) = 4.0;
instance uniform float feather_intensity : hint_range(0.0, 1.0) = 1.0;
instance uniform float near_fade_start : hint_range(0.0, 50.0) = 0.01;
instance uniform float near_fade_end : hint_range(0.0, 50.0) = 1.0;
instance uniform float far_fade_start : hint_range(0.1, 100.0) = 25.0;
instance uniform float far_fade_end : hint_range(0.1, 100.0) = 80.0;
instance uniform float max_brightness : hint_range(1.0, 50.0) = 10.0;
instance uniform bool depth_feather_enabled = true;
instance uniform float depth_fade_distance : hint_range(0.01, 5.0) = 0.5;
instance uniform float cone_top_radius = 0.02;
instance uniform float cone_bottom_radius = 0.8;
instance uniform float cone_height = 8.0;

varying vec3 vertex_view;
varying vec3 local_pos;
varying vec3 local_camera;

float gobo_curve(float sample_value) {
	float clamped_sample = pow(max(sample_value, 0.0001), gobo_gamma);
	return mix(1.0, clamped_sample, gobo_strength);
}

float sample_gobo_plane(vec2 plane_uv) {
	float c = cos(gobo_rotation_rad);
	float s = sin(gobo_rotation_rad);
	mat2 rot = mat2(c, -s, s, c);
	vec2 rotated = rot * plane_uv;
	vec2 tex_uv = rotated * 0.5 + vec2(0.5);
	if (tex_uv.x < 0.0 || tex_uv.x > 1.0 || tex_uv.y < 0.0 || tex_uv.y > 1.0) {
		return 0.0;
	}
	float raw = textureLod(gobo_texture, tex_uv, gobo_mip_bias).r;
	return gobo_curve(raw);
}

float sample_gobo_volume_slice() {
	float axial01 = clamp((local_pos.y / max(cone_height, 0.0001)) + 0.5, 0.0, 1.0);
	float slice_radius = mix(cone_top_radius, cone_bottom_radius, axial01);
	if (slice_radius <= 0.0001) {
		return 1.0;
	}

	vec2 p2 = local_pos.xz;
	vec2 view2 = normalize(local_pos.xz - local_camera.xz);
	if (length(view2) < 0.0001) {
		return sample_gobo_plane(p2 / slice_radius);
	}

	float b = dot(p2, view2);
	float c = dot(p2, p2) - (slice_radius * slice_radius);
	float disc = max((b * b) - c, 0.0);
	float sqrt_disc = sqrt(disc);
	float t0 = -b - sqrt_disc;
	float t1 = -b + sqrt_disc;
	float span = max(t1 - t0, 0.0001);

	const int STEPS = 5;
	float accum = 0.0;
	for (int i = 0; i < STEPS; i++) {
		float f = (float(i) + 0.5) / float(STEPS);
		float t = mix(t0, t1, f);
		vec2 q2 = p2 + view2 * t;
		float radial = length(q2) / slice_radius;
		if (radial <= 1.0) {
			accum += sample_gobo_plane(q2 / slice_radius);
		}
	}

	float avg = accum / float(STEPS);
	float path_gain = clamp(span / max(slice_radius * 2.0, 0.0001), 0.25, 1.0);
	return mix(avg, avg * path_gain, 0.45);
}

void vertex() {
	vertex_view = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	local_pos = VERTEX;
	local_camera = (inverse(MODEL_MATRIX) * vec4(CAMERA_POSITION_WORLD, 1.0)).xyz;
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

	float gobo_multiplier = 1.0;
	if (gobo_enabled) {
		gobo_multiplier = sample_gobo_volume_slice();
	}

	float final_alpha = feather_alpha * depth_mult * dist_fade * gobo_multiplier;
	ALBEDO = base_color.rgb * brightness * gobo_multiplier;
	ALPHA = clamp(final_alpha * base_color.a, 0.0, 1.0);
	EMISSION = ALBEDO * ALPHA;
}
