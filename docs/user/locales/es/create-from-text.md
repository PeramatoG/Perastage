# Create from text

Utiliza **Tools → Create from text** para convertir texto de un rider en una escena inicial. Revisa el resultado antes de utilizarlo como documentación del espectáculo.

## Entradas y flujo de trabajo

Se admite texto pegado y archivos `.txt` y `.pdf`; de los PDF se extrae el texto antes de analizarlo.

1. Abre **Tools → Create from text** y pega texto o carga un rider.
2. Elige **Apply filter** para limpiar las líneas candidatas; después, revisa y edita el texto filtrado.
3. Elige **Create**.
4. Si se abre **Resolve fixture types** porque hay definiciones pendientes de resolver, revisa las propuestas y elige **Resolve and create** para confirmar el plan e iniciar las descargas seleccionadas.

## Resolver tipos de luminaria

Los tipos conocidos utilizan el diccionario de luminarias activo. Para los tipos sin resolver, revisa las sugerencias en caché de GDTF Share, utiliza **Use suggested** si hay una coincidencia clara o **Search...** para elegir un perfil. Puedes corregir un tipo en la tabla y desmarcar **Create** para omitir las luminarias de esa fila y conservar los demás elementos del rider.

Abrir la revisión no requiere iniciar sesión. La primera descarga del catálogo o de un perfil puede requerirlo. Las asignaciones aceptadas solo se recuerdan para futuros riders tras confirmar. Cancelar antes de crear conserva la escena y las asignaciones del diccionario.

### Alternativa genérica

**Use generic** permite usar una alternativa solo para esta importación, sin guardar una asignación ficticia. La falta de catálogo, cancelar el inicio de sesión o un fallo de red no impiden crear con Generic. Los errores recuperables de descarga, validación, modo, autenticación o diccionario solo aplican esta alternativa al tipo afectado; el resto de la importación continúa.

### Modos DMX

Elige explícitamente el modo previsto si el perfil tiene varios. Haz doble clic en **Mode** para revisar las opciones. Las sugerencias sin modo elegido utilizan Generic. Los cambios de modo confirmados en el diccionario se guardan después de **Resolve and create**.

## Contenido generado y patrones de análisis

Según el texto, se crean luminarias, trusses, motores y objetos como pantallas. Las entradas de pantallas o control de vídeo no se tratan como luminarias.

Son útiles las cantidades (`12 Spot`), líneas combinadas (`8 Wash + 4 Beam`), posiciones como `LX1`, `FLOOR` y `SIDES`, y entradas de trusses, barras o motores. Las [preferencias](preferences.md#importación) de Rider Import controlan el patch automático, la organización de capas y la posición LX predeterminada.

## Autocompletado y validación

El editor sugiere palabras clave habituales y términos del diccionario local. Utiliza Up/Down para recorrerlos, Enter o Tab para aceptar y Esc para cerrar las sugerencias.

Después de crear, comprueba las tablas Fixtures, Trusses, Hoists y Objects, los modos DMX y el patch. Verifica la colocación en [2D y 3D](views.md), guarda en PSTG y [exporta MVR](opening-mvr-files.md#exportar-mvr) si lo necesitas.

Para los contratos detallados de análisis y colocación, consulta las [reglas de texto a escena](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/text_to_scene_rules.md).
