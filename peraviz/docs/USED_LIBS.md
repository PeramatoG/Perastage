# Peraviz: módulos de Perastage reutilizados

## Librerías / módulos usados en el hito de proxies MVR

- `models/types.h` y `models/matrixutils.h`  
  Se usan para parsear matrices MVR (`MatrixUtils::ParseMatrix`), componer transformaciones jerárquicas (`MatrixUtils::Multiply`) y extraer euler para proxy placement.

- Esquema MVR de `mvr/` (compatibilidad de estructura XML)  
  El cargador nativo de Peraviz sigue la misma estructura de nodos de Perastage (`GeneralSceneDescription -> Scene -> Layers -> ChildList`) para fixtures, trusses, supports y scene objects.

## Notas

- En este hito todavía no se reutiliza la tubería completa de geometría/materiales; solamente se comparte la base de transformaciones y parsing espacial para validar ejes/unidades.
