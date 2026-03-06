shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never;

uniform sampler2D DEPTH_TEXTURE : hint_depth_texture, filter_linear_mipmap;

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
uniform sampler2D gobo_texture : source_color;
uniform bool use_gobo = false;
uniform float gobo_contrast : hint_range(0.1, 4.0) = 1.0;
uniform bool gobo_invert = false;
instance uniform float cone_height = 15.0;
instance uniform float gobo_projection_radius = 1.0;


varying vec3 v_view;
varying vec3 v_local;

vec2 project_gobo_uv(vec3 local_pos) {
	float source_y = 0.5 * cone_height;
	float axial_dist = max(source_y - local_pos.y, 0.001);
	float projection_scale = cone_height / axial_dist;
	float projection_radius = max(gobo_projection_radius, 0.001);
	vec2 projected = local_pos.xz * projection_scale;
	return vec2(0.5) + (projected / (2.0 * projection_radius));
}


float sample_gobo_mask(vec2 uv) {
	vec2 safe_uv = clamp(uv, vec2(0.0), vec2(1.0));
	vec4 texel = texture(gobo_texture, safe_uv);
	float luma = clamp(dot(texel.rgb, vec3(0.299, 0.587, 0.114)), 0.0, 1.0);
	float alpha = clamp(texel.a, 0.0, 1.0);
	float open_amount = luma;
	if (gobo_invert) {
		open_amount = 1.0 - luma;
	}
	return max(pow(clamp(open_amount * alpha, 0.0, 1.0), gobo_contrast), 0.06);
}

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
	if (final_alpha < 0.001) {
		discard;
	}

	float gobo_mask = 1.0;
	if (use_gobo) {
		vec2 gobo_uv = project_gobo_uv(v_local);
		gobo_mask = sample_gobo_mask(gobo_uv);
	}

	ALBEDO = base_color.rgb * brightness * gobo_mask;
	ALPHA = clamp(final_alpha * base_color.a * gobo_mask, 0.0, 1.0);
	EMISSION = ALBEDO * ALPHA;
}
