shader_type spatial;

render_mode unshaded, blend_mix, cull_disabled, depth_draw_never;

uniform sampler2D gobo_texture : hint_albedo;
uniform float mask_cutoff : hint_range(0.0, 1.0) = 0.5;
uniform float edge_softness : hint_range(0.0, 0.2) = 0.01;
uniform float radial_softness : hint_range(0.0, 0.2) = 0.03;
uniform float mask_strength : hint_range(0.0, 1.0) = 0.85;

void fragment() {
	vec4 gobo_sample = texture(gobo_texture, UV);
	float gobo_luma = dot(gobo_sample.rgb, vec3(0.299, 0.587, 0.114));
	float pass_mask = smoothstep(mask_cutoff - edge_softness, mask_cutoff + edge_softness, gobo_luma);

	float radial_distance = length(UV - vec2(0.5));
	float radial_falloff = 1.0 - smoothstep(0.5 - radial_softness, 0.5, radial_distance);
	float block_alpha = (1.0 - pass_mask) * radial_falloff * mask_strength;

	ALBEDO = vec3(0.0);
	ALPHA = clamp(block_alpha, 0.0, 1.0);
	EMISSION = vec3(0.0);
}
