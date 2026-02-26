shader_type spatial;
render_mode blend_mix, depth_prepass_alpha, cull_disabled, unshaded;

uniform sampler2D gobo_texture : source_color;
uniform float alpha_cutoff : hint_range(0.0, 1.0) = 0.5;
uniform vec2 uv_offset = vec2(0.0, 0.0);
uniform vec2 uv_scale = vec2(1.0, 1.0);
uniform float bw_threshold : hint_range(0.0, 1.0) = 0.5;

void fragment() {
	vec2 fitted_uv = uv_offset + (UV * uv_scale);
	vec2 circle_uv = (UV - vec2(0.5)) / 0.5;
	float outside_circle = step(1.0, length(circle_uv));

	vec4 gobo_sample = vec4(0.0);
	if (fitted_uv.x >= 0.0 && fitted_uv.y >= 0.0 && fitted_uv.x <= 1.0 && fitted_uv.y <= 1.0) {
		gobo_sample = texture(gobo_texture, fitted_uv);
	}

	float luma = dot(gobo_sample.rgb, vec3(0.299, 0.587, 0.114));
	float gobo_blocker = (1.0 - step(bw_threshold, luma)) * gobo_sample.a;
	float blocker = clamp(max(gobo_blocker, outside_circle), 0.0, 1.0);

	ALBEDO = mix(vec3(1.0), vec3(0.0), blocker);
	ALPHA = blocker;
	ALPHA_SCISSOR_THRESHOLD = alpha_cutoff;
}
