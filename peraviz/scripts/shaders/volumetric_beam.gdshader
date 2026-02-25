shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never;

uniform sampler2D DEPTH_TEXTURE : hint_depth_texture, filter_linear_mipmap;

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

varying vec3 vertex_view;

void vertex() {
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
	ALBEDO = base_color.rgb * brightness;
	ALPHA = clamp(final_alpha * base_color.a, 0.0, 1.0);
	EMISSION = ALBEDO * ALPHA;
}
