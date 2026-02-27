shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_lambert, specular_schlick_ggx;

uniform sampler2D gobo_texture;
instance uniform float gobo_cutoff : hint_range(0.0, 1.0) = 0.5;
instance uniform vec2 gobo_size = vec2(0.1, 0.1);
instance uniform float gobo_thickness : hint_range(0.0005, 0.05) = 0.003;

void fragment() {
	vec2 uv = (UV - vec2(0.5)) * (vec2(0.1) / max(gobo_size, vec2(0.001))) + vec2(0.5);
	vec4 tex = texture(gobo_texture, uv);
	float luma = dot(tex.rgb, vec3(0.299, 0.587, 0.114)) * tex.a;
	ALPHA = 1.0 - luma;
	ALPHA_SCISSOR_THRESHOLD = gobo_cutoff;
}
