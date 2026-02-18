shader_type spatial;
render_mode unshaded, blend_add, cull_back, depth_draw_never;

instance uniform vec4 beam_color : source_color = vec4(1.0, 0.85, 0.55, 1.0);
instance uniform float beam_intensity = 1.0;
instance uniform float cone_height = 20.0;
instance uniform float top_radius = 0.02;
instance uniform float bottom_radius = 0.8;
instance uniform float haze_density = 0.17;
instance uniform float anisotropy = 0.62;
instance uniform float noise_amount = 0.05;
instance uniform float noise_scale = 1.4;
instance uniform float end_fade_ratio = 0.82;
instance uniform float step_count = 24.0;
instance uniform float beam_range = 20.0;
instance uniform float camera_inside_dimmer = 0.5;

uniform sampler2D DEPTH_TEXTURE : hint_depth_texture;

varying vec3 local_position;
varying vec3 world_position;

float hash31(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

float value_noise(vec3 p) {
	vec3 ip = floor(p);
	vec3 fp = fract(p);
	fp = fp * fp * (3.0 - 2.0 * fp);

	float n000 = hash31(ip + vec3(0.0, 0.0, 0.0));
	float n100 = hash31(ip + vec3(1.0, 0.0, 0.0));
	float n010 = hash31(ip + vec3(0.0, 1.0, 0.0));
	float n110 = hash31(ip + vec3(1.0, 1.0, 0.0));
	float n001 = hash31(ip + vec3(0.0, 0.0, 1.0));
	float n101 = hash31(ip + vec3(1.0, 0.0, 1.0));
	float n011 = hash31(ip + vec3(0.0, 1.0, 1.0));
	float n111 = hash31(ip + vec3(1.0, 1.0, 1.0));

	float nx00 = mix(n000, n100, fp.x);
	float nx10 = mix(n010, n110, fp.x);
	float nx01 = mix(n001, n101, fp.x);
	float nx11 = mix(n011, n111, fp.x);
	float nxy0 = mix(nx00, nx10, fp.y);
	float nxy1 = mix(nx01, nx11, fp.y);
	return mix(nxy0, nxy1, fp.z);
}

float phase_hg(float cos_theta, float g) {
	float gg = g * g;
	float denom = pow(max(1.0 + gg - 2.0 * g * cos_theta, 0.001), 1.5);
	return (1.0 - gg) / (12.5663706 * denom);
}

void vertex() {
	local_position = VERTEX;
	world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	float scene_depth = texture(DEPTH_TEXTURE, SCREEN_UV).r;
	float fragment_depth = FRAGCOORD.z;
	float depth_occlusion = smoothstep(0.0, 0.002, scene_depth - fragment_depth);

	float axial = clamp((local_position.y / max(cone_height, 0.001)) + 0.5, 0.0, 1.0);
	float current_radius = mix(top_radius, bottom_radius, axial);
	float radial = length(vec2(local_position.x, local_position.z));
	float radial_ratio = clamp(radial / max(current_radius, 0.0001), 0.0, 1.0);

	float radial_density = 1.0 - smoothstep(0.55, 1.0, radial_ratio);
	float axial_density = mix(1.25, 0.35, axial);
	float end_fade = 1.0 - smoothstep(end_fade_ratio, 1.0, axial);

	vec3 view_dir = normalize(CAMERA_POSITION_WORLD - world_position);
	vec3 light_dir = normalize((MODEL_MATRIX * vec4(0.0, -1.0, 0.0, 0.0)).xyz);
	float cos_theta = dot(view_dir, light_dir);
	float phase = phase_hg(cos_theta, anisotropy);

	float steps = max(step_count, 1.0);
	float step_length = beam_range / steps;
	float sigma_t = haze_density;
	float sigma_s = haze_density * 0.7;
	float transmittance = 1.0;
	float radiance = 0.0;

	for (float i = 0.0; i < 64.0; i += 1.0) {
		if (i >= steps) {
			break;
		}
		float t = (i + 0.5) / steps;
		float local_axial = clamp(mix(axial, 1.0, t * 0.7), 0.0, 1.0);
		float local_end_fade = 1.0 - smoothstep(end_fade_ratio, 1.0, local_axial);
		float sample_noise = 1.0;
		if (noise_amount > 0.001) {
			sample_noise += (value_noise(vec3(local_position.xz, local_axial) * noise_scale + vec3(TIME * 0.07)) - 0.5) * noise_amount;
		}
		float sample_density = max(radial_density * axial_density * local_end_fade * sample_noise, 0.0);
		transmittance *= exp(-sigma_t * sample_density * step_length);
		radiance += transmittance * sigma_s * sample_density * phase * step_length;
		if (transmittance < 0.02) {
			break;
		}
	}

	float camera_inside = step(radial_ratio, 1.0) * step(axial, 1.0) * step(0.0, axial);
	float inside_dimmer = mix(1.0, camera_inside_dimmer, camera_inside);
	float final_alpha = clamp(radiance * beam_intensity * depth_occlusion * inside_dimmer, 0.0, 1.0) * end_fade;

	ALBEDO = beam_color.rgb;
	EMISSION = beam_color.rgb * final_alpha * 3.5;
	ALPHA = final_alpha;
}
