shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_prepass_alpha, shadows_disabled;

instance uniform vec4 beam_color : source_color = vec4(1.0, 0.85, 0.55, 1.0);
instance uniform float beam_intensity = 1.0;
instance uniform float cone_height = 20.0;
instance uniform float top_radius = 0.02;
instance uniform float bottom_radius = 0.8;
instance uniform float haze_density = 0.22;
instance uniform float anisotropy = 0.62;
instance uniform float noise_amount = 0.05;
instance uniform float noise_scale = 1.4;
instance uniform float end_fade_ratio = 0.82;
instance uniform float step_count = 24.0;
instance uniform float beam_range = 20.0;
instance uniform float camera_inside_dimmer = 0.6;

varying float beam_axial;
varying vec3 local_position;

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
	float normalized_y = (VERTEX.y / max(cone_height, 0.0001)) + 0.5;
	beam_axial = clamp(normalized_y, 0.0, 1.0);
	local_position = VERTEX;
}

void fragment() {
	float near_factor = beam_axial;
	float far_progress = 1.0 - near_factor;
	float end_fade = 1.0 - smoothstep(clamp(end_fade_ratio, 0.0, 0.99), 1.0, far_progress);
	float current_radius = mix(top_radius, bottom_radius, beam_axial);
	float radial = length(vec2(local_position.x, local_position.z));
	float radial_ratio = clamp(radial / max(current_radius, 0.0001), 0.0, 1.0);
	float radial_density = 1.0 - smoothstep(0.62, 1.0, radial_ratio);

	float view_alignment = abs(dot(normalize(NORMAL), normalize(VIEW)));
	float phase = phase_hg(view_alignment, anisotropy);

	float steps = clamp(step_count, 8.0, 64.0);
	float step_length = beam_range / steps;
	float sigma_t = haze_density;
	float sigma_s = haze_density * 0.8;
	float transmittance = 1.0;
	float radiance = 0.0;

	for (float i = 0.0; i < 64.0; i += 1.0) {
		if (i >= steps) {
			break;
		}
		float t = (i + 0.5) / steps;
		float sample_far_progress = clamp(mix(far_progress, 1.0, t * 0.9), 0.0, 1.0);
		float sample_end_fade = 1.0 - smoothstep(clamp(end_fade_ratio, 0.0, 0.99), 1.0, sample_far_progress);
		float sample_axial_density = mix(1.25, 0.28, sample_far_progress);
		float sample_noise = 1.0;
		if (noise_amount > 0.001) {
			sample_noise += (value_noise(vec3(local_position.xz * noise_scale, sample_far_progress + TIME * 0.08)) - 0.5) * noise_amount;
		}
		float density = max(radial_density * sample_axial_density * sample_end_fade * sample_noise, 0.0);
		transmittance *= exp(-sigma_t * density * step_length);
		radiance += transmittance * sigma_s * density * phase * step_length;
		if (transmittance < 0.02) {
			break;
		}
	}

	float inside_dimmer = mix(1.0, camera_inside_dimmer, smoothstep(0.0, 0.2, radial_ratio));
	float alpha = clamp(radiance * beam_intensity * inside_dimmer * 18.0, 0.0, 1.0) * end_fade;

	float core_whitening = pow(max(1.0 - radial_ratio, 0.0), 3.2) * smoothstep(0.0, 0.35, near_factor);
	vec3 scatter_color = mix(vec3(1.0), beam_color.rgb, 0.35);
	vec3 emission_color = mix(scatter_color, vec3(1.0), clamp(core_whitening, 0.0, 1.0));
	ALBEDO = scatter_color;
	EMISSION = emission_color * alpha * 2.2;
	ALPHA = alpha;
}
