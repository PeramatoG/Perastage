shader_type fog;

uniform sampler2D gobo_texture : source_color;
uniform float density_scale : hint_range(0.0, 10.0) = 0.9;
uniform float emission_strength : hint_range(0.0, 10.0) = 0.55;
uniform vec4 light_color : source_color = vec4(1.0);
uniform float edge_softness : hint_range(0.1, 1.0) = 0.72;
uniform bool invert_gobo = false;
uniform float gobo_scale : hint_range(0.05, 8.0) = 1.0;
uniform float gobo_rotation_deg : hint_range(-360.0, 360.0) = 0.0;
uniform float radial_falloff : hint_range(0.05, 4.0) = 1.25;
uniform float longitudinal_falloff : hint_range(0.05, 4.0) = 1.1;

vec2 rotate2d(vec2 point, float angle_rad) {
	float c = cos(angle_rad);
	float s = sin(angle_rad);
	return vec2((point.x * c) - (point.y * s), (point.x * s) + (point.y * c));
}

void fog() {
	float depth_t = clamp(1.0 - UVW.z, 0.001, 1.0);
	vec2 projected = (UVW.xy - vec2(0.5)) / depth_t;
	projected = rotate2d(projected / max(gobo_scale, 0.05), radians(gobo_rotation_deg));
	vec2 projected_uv = projected * 0.5 + vec2(0.5);

	if (projected_uv.x < 0.0 || projected_uv.x > 1.0 || projected_uv.y < 0.0 || projected_uv.y > 1.0) {
		DENSITY = 0.0;
		ALBEDO = vec3(0.0);
		EMISSION = vec3(0.0);
		return;
	}

	vec4 gobo_sample = texture(gobo_texture, projected_uv);
	float gobo_intensity = clamp(dot(gobo_sample.rgb, vec3(0.299, 0.587, 0.114)) * gobo_sample.a, 0.0, 1.0);
	if (invert_gobo) {
		gobo_intensity = 1.0 - gobo_intensity;
	}

	float radial = length(projected);
	float radial_mask = pow(clamp(1.0 - radial, 0.0, 1.0), max(radial_falloff, 0.05));
	float edge_mask = smoothstep(edge_softness, edge_softness - 0.1, radial);
	float longitudinal_mask = pow(depth_t, max(longitudinal_falloff, 0.05));
	float final_mask = radial_mask * edge_mask * longitudinal_mask;

	DENSITY = gobo_intensity * density_scale * final_mask;
	ALBEDO = light_color.rgb;
	EMISSION = light_color.rgb * emission_strength * gobo_intensity * final_mask;
}
