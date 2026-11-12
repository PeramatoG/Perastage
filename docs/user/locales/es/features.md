# Funciones

Perastage es una herramienta de escritorio de código abierto para trabajar con escenas de iluminación y rigging y elaborar documentación técnica.

## Proyectos MVR

[Abre, fusiona y exporta MVR](opening-mvr-files.md), revisa conflictos y guarda el trabajo en curso como proyectos PSTG.

## GDTF y GDTF Share

Utiliza definiciones de luminarias y diccionarios locales, [busca y descarga GDTF](gdtf-download.md), consulta el catálogo en caché sin conexión y revisa revisiones y modos DMX. El editor inspecciona cada Model y conserva los recursos originales GDTF.

## MVR / GDTF Inspector

[Inspecciona](inspector.md) instantáneas del proyecto y paquetes externos sin editarlos. Explora árboles de escena y paquete, definiciones de luminarias, XML, vistas previas e incidencias detectadas.

## Visualización 2D y 3D

[Revisa planos y relaciones espaciales](views.md), elige estilos de renderizado y utiliza controles de movimiento, ejes y ajuste. Adapta el detalle de los modelos para escenas grandes.

## Edición de escena, tablas, patch y grupos

Edita luminarias, trusses, motores y objetos; revisa metadatos y patch; organiza capas y grupos. Utiliza la edición en tabla, la colocación continua, Undo/Redo y [Console](shortcuts-and-command-bar.md).

## Create from text

[Genera escenas iniciales](create-from-text.md) desde texto pegado o riders TXT/PDF, revisa las coincidencias de tipos de luminaria, elige modos o continúa con Generic.

## Layouts y documentación PDF

[Prepara páginas imprimibles](layouts.md) con vistas 2D, leyendas, tablas de evento, texto e imágenes. Comparte plantillas y exporta layouts a PDF.

## Integración local experimental

OSC y [MCP](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/mcp_adapter.md) ofrecen integración local experimental para automatización con configuración independiente. Los contratos técnicos pertenecen a la [documentación para desarrolladores](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/index.md).

## Plataformas y código fuente

Perastage admite Windows, macOS y Linux y utiliza la licencia GNU GPL versión 3 o posterior. Consulta [Instalación](installation.md) para los paquetes y el [repositorio](https://github.com/PeramatoG/Perastage) para el código y las contribuciones.

## Patch DMX manual

La edición manual de una luminaria conserva un inicio Universe/Channel válido (canal 1–512), aunque su footprint GDTF primario exceda el universo. Una celda Channel roja explica al pasar el ratón el desbordamiento, footprint desconocido o solapamiento. Por ejemplo, 180 canales en 1.400 abarcarían 1.400–1.512 y 2.1–2.67 con un hipotético Auto Span continuo. Es información: MVR estándar almacena el inicio y no garantiza ese enrutamiento; debe admitirlo la aplicación receptora o controladora. Los breaks secundarios se conservan. AutoPatch y la asignación secuencial múltiple mantienen sus reglas de salto de universo.

## Visor DMX Patch

Abre **Ver → DMX Patch** para consultar los 512 canales de cualquier universo debajo del visor 2D/3D activo. Las celdas compactas y el número de columnas se ajustan automáticamente al ancho disponible; introduce un número de universo para consultar uno vacío. Los colores siguen la presentación de los aparatos y las etiquetas muestran atributos físicos GDTF; la información al pasar el ratón identifica todos los aparatos y breaks. Las celdas rayadas indican solapamientos. La lista de diagnósticos explica definiciones ausentes, asignaciones y desbordamientos; no se continúa automáticamente al siguiente universo. El visor es de solo lectura. Al cerrarlo recuperas el espacio del visor; se recuerdan su visibilidad y altura. Se oculta temporalmente en los modos Layout e Inspector. Usa los controles de la cabecera para cerrar o maximizar el panel. El icono de aviso abre una lista desplazable de diagnósticos en lugar de un tooltip largo.

El nombre de cada aparato aparece solo en su primera fila; los números centrados, el primer canal enmarcado y las esquinas redondeadas identifican sus límites. **Offsets declarados** está activado por defecto para definiciones ambiguas: los bytes provisionales se muestran respecto al inicio primario, con borde discontinuo e interrogación y un contador separado de la ocupación y los conflictos confirmados. Esta opción no corrige breaks, reescribe archivos ni continúa en otro universo.
