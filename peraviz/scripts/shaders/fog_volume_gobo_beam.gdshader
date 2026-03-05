shader_type fog;

uniform sampler2D gobo_texture : source_color;
uniform float density_scale : hint_range(0.0, 10.0) = 1.0;
uniform float emission_strength : hint_range(0.0, 10.0) = 1.0;
uniform vec4 light_color : source_color = vec4(1.0);
uniform float edge_softness : hint_range(0.1, 1.0) = 0.8;
uniform bool invert_gobo = false;

void fog() {
	float depth_t = max(1.0 - UVW.z, 0.001);
	vec2 projected_uv = (UVW.xy - vec2(0.5)) / depth_t;
	projected_uv = projected_uv * 0.5 + vec2(0.5);

	if (projected_uv.x < 0.0 || projected_uv.x > 1.0 || projected_uv.y < 0.0 || projected_uv.y > 1.0) {
		DENSITY = 0.0;
		ALBEDO = vec3(0.0);
		EMISSION = vec3(0.0);
		return;
	}

	vec4 gobo_sample = texture(gobo_texture, projected_uv);
	float gobo_intensity = clamp(gobo_sample.r, 0.0, 1.0);
	if (invert_gobo) {
		gobo_intensity = 1.0 - gobo_intensity;
	}

	float radial = length((UVW.xy - vec2(0.5)) / depth_t);
	float mask = smoothstep(edge_softness, edge_softness - 0.1, radial);

	DENSITY = gobo_intensity * density_scale * mask;
	ALBEDO = light_color.rgb;
	EMISSION = light_color.rgb * emission_strength * gobo_intensity * mask;
}
