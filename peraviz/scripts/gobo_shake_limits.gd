extends RefCounted
class_name GoboShakeLimits

const MAX_SHAKE_AMPLITUDE_DEG: float = 1.0
const MAX_SHAKE_FREQUENCY_HZ: float = 7.0
const DEFAULT_DEBUG_SHAKE_AMPLITUDE_DEG: float = 1.0
const DEFAULT_DEBUG_SHAKE_FREQUENCY_HZ: float = 1.0

static func clamp_amplitude_deg(value: float) -> float:
	return clamp(absf(value), 0.0, MAX_SHAKE_AMPLITUDE_DEG)

static func clamp_frequency_hz(value: float) -> float:
	return clamp(absf(value), 0.0, MAX_SHAKE_FREQUENCY_HZ)
