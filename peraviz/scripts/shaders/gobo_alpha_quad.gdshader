shader_type spatial;
render_mode blend_mix, depth_draw_alpha_prepass, cull_disabled, unshaded;

uniform sampler2D gobo_texture : source_color;
uniform float alpha_cutoff : hint_range(0.0, 1.0) = 0.25;
uniform vec2 uv_offset = vec2(0.0, 0.0);
uniform vec2 uv_scale = vec2(1.0, 1.0);

void fragment() {
	vec2 transformed_uv = uv_offset + (UV * uv_scale);
	if (transformed_uv.x < 0.0 || transformed_uv.y < 0.0 || transformed_uv.x > 1.0 || transformed_uv.y > 1.0) {
		ALBEDO = vec3(0.0);
		ALPHA = 0.0;
		ALPHA_SCISSOR_THRESHOLD = alpha_cutoff;
		return;
	}
	vec4 gobo_sample = texture(gobo_texture, transformed_uv);
	float luma = dot(gobo_sample.rgb, vec3(0.299, 0.587, 0.114));
	float alpha_mask = clamp(1.0 - luma, 0.0, 1.0);
	ALBEDO = vec3(0.0);
	ALPHA = alpha_mask;
	ALPHA_SCISSOR_THRESHOLD = alpha_cutoff;
}
