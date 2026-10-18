# Preferencias

Abre **Edit → Preferences**. Cambiar de categoría conserva los cambios pendientes. **Apply** guarda sin cerrar, **OK** guarda y cierra, y **Cancel** descarta los cambios desde el último Apply.

## General

- **Language:** elige el idioma de la interfaz; reinicia Perastage para aplicarlo.
- **Units:** elige las unidades de distancia y peso. Los cambios se aplican inmediatamente.
- **Updates:** **Check on startup (recommended)** comprueba las actualizaciones como máximo una vez cada 24 horas. Puedes ocultar los recordatorios de una versión concreta. **Manual only** desactiva la comprobación al iniciar; **Help → Check for Updates** permite comprobarlas cuando quieras.

### Caché

**General → Caché** muestra la ubicación de solo lectura y el espacio ocupado.
**Abrir carpeta de caché** abre la carpeta en el gestor de archivos del sistema.
**Vaciar caché...** pide confirmación y elimina inmediatamente las cachés
regenerables de Perastage, independientemente de Aplicar, Aceptar o Cancelar.
La geometría se regenera cuando hace falta y el catálogo GDTF vuelve a descargarse
cuando hay conexión. Se conservan la configuración, los registros, las
credenciales, las bibliotecas, los archivos GDTF, los diccionarios y los proyectos.
Si algún archivo no puede eliminarse, Perastage informa del fallo y actualiza el
tamaño mostrado. Las cachés persistentes se guardan en `cache/` dentro de los
datos de aplicación del usuario, en lugar de junto a los archivos originales.

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

### Recursos GDTF

Las preferencias GDTF gestionan las credenciales de Share. Perastage guarda
automáticamente los símbolos Top/Front/Side/Bottom de la luminaria completa en
PSTG. Guarda el proyecto para conservarlos, incluidas las sustituciones del
usuario; no se convierten en recursos GDTF ni MVR. El editor permite inspeccionar
los SVG originales de cada Model y conserva sus bytes y offsets. La edición
explícita de propiedades físicas sigue disponible.

### Importación / exportación MVR

**Cerrar automáticamente la ventana de descarga de GDTF al finalizar** está desactivado de forma predeterminada, también en instalaciones existentes. La cola conserva los estados, colores, progreso y resumen finales hasta que pulses **Aceptar** o cierres la ventana; entonces continúa la importación. Activa esta opción para continuar automáticamente. Durante las descargas, **Cancelar** o el botón de cierre solicita la cancelación: se conservan las descargas completadas y las luminarias restantes usan las alternativas existentes. La preferencia se guarda entre reinicios y es independiente de las credenciales de GDTF Share.

**Truss geometry export mode** ofrece:

- **Standard MVR representation** (predeterminado): conserva las referencias a símbolos importados cuando es posible.
- **Direct Geometry3D for truss symbols:** expande las referencias a símbolos de truss para aplicaciones que necesitan geometría directa.

Ambas generan MVR compatible con 1.6. Guardar el proyecto conserva la representación estándar. Las opciones de importación se describen en [Abrir archivos MVR](opening-mvr-files.md).
