# Auditoría de implementación — refactor de `ConfigManager`

Fecha: 2026-02-12

## Resultado ejecutivo

Estado global: **parcialmente implementado**.

## 1) Extracción desde `core/configmanager.*`

### Implementado

Se encontraron los cinco subservicios extraídos en `core/configservices.h` y su implementación en `core/configservices.cpp`:

- `UserPreferencesStore`
- `ProjectSession`
- `SelectionState`
- `HistoryManager`
- `LayerVisibilityState`

Además, `ConfigManager` compone estos servicios y delega múltiples operaciones a ellos.

### Observación importante

Aunque `ProjectSession` ya centraliza `scene` + `dirty flag` (`revision/savedRevision`), la lógica de **load/save de proyecto** sigue en `ConfigManager::SaveProject` y `ConfigManager::LoadProject`, no en `ProjectSession`.

## 2) `ConfigManager` como façade de compatibilidad

Implementado.

`ConfigManager` mantiene la API histórica y delega en los subservicios internos (`preferencesStore`, `projectSession`, `selectionState`, `historyManager`, `layerVisibilityState`), actuando como façade de compatibilidad.

## 3) Sustitución gradual de `ConfigManager::Get()` en GUI por dependencias inyectadas

Implementación **incompleta**.

Hay evidencia de algunos puntos con dependencia pasada por referencia/puntero (`viewer2dstate`), pero persisten muchas llamadas directas al singleton en GUI.

Conteo actual en `gui/`: **112** ocurrencias de `ConfigManager::Get()`.

Conclusión: el reemplazo “gradual” puede haber empezado, pero no está cercano a completarse.

## 4) Tests de unidad para cada subservicio extraído

Implementado.

Existen tests dedicados para cada subservicio:

- `tests/user_preferences_store_test.cpp`
- `tests/project_session_test.cpp`
- `tests/selection_state_test.cpp`
- `tests/history_manager_test.cpp`
- `tests/layer_visibility_state_test.cpp`

También están registrados en `tests/CMakeLists.txt` con ejecutables y `add_test(...)`.

## Veredicto final

La iniciativa está avanzada y bien encaminada, pero **no está completamente implementada** por dos razones principales:

1. La responsabilidad de load/save de proyecto sigue dentro de `ConfigManager` en lugar de `ProjectSession`.
2. La GUI todavía depende ampliamente de `ConfigManager::Get()`.
