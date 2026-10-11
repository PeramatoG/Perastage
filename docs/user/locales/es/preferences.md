# Preferencias

Abre **Edit → Preferences**. Cambiar de categoría conserva los cambios pendientes. **Apply** guarda sin cerrar, **OK** guarda y cierra, y **Cancel** descarta los cambios desde el último Apply.

## General

- **Language:** elige el idioma de la interfaz; reinicia Perastage para aplicarlo.
- **Units:** elige las unidades de distancia y peso. Los cambios se aplican inmediatamente.
- **Updates:** **Check on startup (recommended)** comprueba las actualizaciones como máximo una vez cada 24 horas. Puedes ocultar los recordatorios de una versión concreta. **Manual only** desactiva la comprobación al iniciar; **Help → Check for Updates** permite comprobarlas cuando quieras.

## Importación

**Rider Import** controla **Auto patch after import**, las capas automáticas por posición o tipo de luminaria, y las alturas, posiciones y márgenes LX predeterminados. Los campos de distancia siguen las unidades elegidas. Consulta [Create from text](create-from-text.md).

## Visores

### 3D Viewer

Elige la presentación y **Performance → Model detail**:

- **Standard** (predeterminado) equilibra detalle y rendimiento.
- **Low** simplifica los modelos pesados con mayor intensidad.
- **High** muestra todo el detalle disponible del modelo original.

**Use a simplified proxy while navigating** es una opción independiente y desactivada por defecto. Simplifica temporalmente las luminarias no seleccionadas al navegar rápido y después restaura el detalle elegido. Los cambios se aplican en el siguiente redibujado, sin reabrir el proyecto. La geometría original, los recursos GDTF, los símbolos 2D y la geometría exportada se conservan.

### Selección y movimiento

Elige qué tipos de objeto desplazan su grupo raíz durante las transformaciones con ratón, Console y Magnet. Los trusses agrupados mueven su grupo por defecto; luminarias, soportes/motores y objetos de escena se mueven de forma independiente. Seleccionar directamente un grupo lo mueve entero. La edición en tabla solo afecta al objeto editado.

**Show anchor references while moving or inserting elements** muestra anclajes compatibles de Magnet al mover o insertar. Estas guías no forman parte de la escena ni de la exportación. Cambiar las preferencias de movimiento no modifica la jerarquía ni marca el proyecto como editado.

## Formatos

### Completar definiciones GDTF

En **GDTF → GDTF definition completion**:

- **Complete and improve GDTF definitions (recommended)** puede completar vistas estándar Top/Side/Front que falten a partir de la geometría, en un derivado `@Perastage.gdtf` auditado. Conserva las vistas originales existentes, incluidas las inutilizables que requieran reparación explícita.
- **Preserve imported GDTF definitions** desactiva el completado automático.

Ambas opciones permiten editar la escena y los GDTF explícitamente. Las vistas de Perastage aplicadas manualmente son símbolos del proyecto guardados en PSTG, no sustitutos de las vistas originales GDTF. Guarda el proyecto para conservarlas; no se exportan a MVR ni a los GDTF incrustados.

### Importación / exportación MVR

**Truss geometry export mode** ofrece:

- **Standard MVR representation** (predeterminado): conserva las referencias a símbolos importados cuando es posible.
- **Direct Geometry3D for truss symbols:** expande las referencias a símbolos de truss para aplicaciones que necesitan geometría directa.

Ambas generan MVR compatible con 1.6. Guardar el proyecto conserva la representación estándar. Las opciones de importación se describen en [Abrir archivos MVR](opening-mvr-files.md).
