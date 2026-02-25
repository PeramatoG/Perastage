shader_type spatial;

render_mode unshaded, blend_add, cull_disabled, depth_draw_never;

uniform sampler2D gobo_texture : hint_albedo;
uniform float intensity : hint_range(0.0, 3.0) = 1.0;
uniform float radial_softness : hint_range(0.001, 1.0) = 0.08;

void fragment() {
	vec4 gobo_sample = texture(gobo_texture, UV);
	float gobo_luma = dot(gobo_sample.rgb, vec3(0.299, 0.587, 0.114));
	float radial_distance = length(UV - vec2(0.5));
	float radial_falloff = 1.0 - smoothstep(0.5 - radial_softness, 0.5, radial_distance);
	float alpha = gobo_sample.a * gobo_luma * radial_falloff * intensity;
	vec3 gobo_color = gobo_sample.rgb * radial_falloff * intensity;

	ALBEDO = gobo_color;
	ALPHA = alpha;
	EMISSION = gobo_color * alpha;
}
