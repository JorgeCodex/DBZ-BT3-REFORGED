# Iluminación de personajes y escenario en Adventure

## Evidencia del código

En Tenkaichi3Decomp, `src/battle/btl_obj.c`, `BtlObj_UpdateAll` consulta en cada actualización la dirección `BtlStage_GetLightVecB` y cuatro componentes del registro `BtlStage_GetLightColors` (bytes 8–11); después llama a `BtlObjLight_SetDir` y `BtlObjLight_SetColor`. La dirección se normaliza. También se calcula un vector intermedio a partir de la dirección y `(0, -1.6, 0)` en `BtlObjLight_SetDir`. Los valores por defecto de `BtlObjLight_Init` no prueban los valores del escenario activo porque se sobrescriben al actualizar.

El shader del puerto `port/src/gs/shaders/vu0.vert` documenta una capa de sombreado del personaje que consulta una textura usando la normal y una matriz alineada con la cámara. La orientación de los huesos y la normal de cada parte también intervienen. Esto hace plausible que la apariencia varíe con cámara o personaje, pero las capturas no identifican cuál cambio concreto causó la diferencia.

El conversor `tools/map-conversion/convert_earth.py` sustituye la geometría por la de BT2 pero conserva el bloque MAPD de la plantilla de escenario de BT3, con cambios específicos en niebla y partículas. La geometría de BT2 y la configuración de iluminación no proceden necesariamente del mismo escenario.

## Datos del paquete local

Leídos del archivo local `external/local-only/DBZBT3R/earth-adventure-map.unk`, sección MAPD, no de una captura en ejecución:

- Número de registros de luz: 1. Segundo vector del registro en el archivo: `(-0.989710, 0.119989, -0.077953, 1.0)`.
- `BtlStage_BindData` invierte Y y Z al cargar ese vector; el valor esperado antes de normalizar es aproximadamente `(-0.989710, -0.119989, 0.077953)`.
- Hay un registro de colores; sus bytes 8–11 son `(0, 0, 32, 32)`. El código los pasa como cuatro componentes a la luz del objeto. No se ha verificado visualmente el efecto individual de cada componente.

## Límites y continuación

La posición de la cámara y la orientación del personaje pueden influir en la imagen final. Aún no se ha registrado el estado efectivo durante el movimiento ni demostrado cuál parámetro explica las capturas. Para aislarlo: registrar dirección, color, posición, orientación y cámara por cuadro; comparar pruebas que varíen solo una de esas variables; investigar cómo aplicar la iluminación al dibujo del escenario sin comprometer texturas ni rendimiento. No se modificó el juego.

## Prueba aislada: dirección de luz y sombreado de vértices

Se ejecutó Adventure con posición fija en seis fases: tres direcciones de cámara con el modelo fijo y tres direcciones del modelo con la cámara fija. En las seis muestras, la luz global de objetos fue idéntica: dirección `(-0.9897, -0.1200, 0.0780)`, vector intermedio `(-0.4984, -0.8661, 0.0393)` y color `(0, 0, 32, 32)`. Esto comprueba que esos valores globales no cambiaron durante la prueba, pero no mide cómo se combina la luz con las normales y la cámara en la imagen final.

Se creó una copia local del paquete y se modificaron 47 442 colores de vértices del grupo visible de terreno y objetos, aplicando una aproximación de iluminación direccional estática con la dirección medida. La copia se cargó y permitió volar en una prueba grabada; no se midió rendimiento ni se validaron todos los lugares. Una comparación del mismo fotograma mostró que el resultado oscurece el escenario, sin demostrar coherencia con el sombreado de personajes ni generar sombras proyectadas. No se instaló en el juego.

Archivos experimentales en el espacio de trabajo local: `work/light-experiment/earth-shaded.unk`, `work/light-experiment/comparacion.png` y registro `work/adventure-own-loader/replay/light-experiment.log`. La prueba no modifica el mapa original.

## Prueba con Blender en una zona limitada

Se abrió la copia `.blend` original con Blender 5.2.3 y se usó su BVH para consultar qué vértices del terreno y objetos en un radio de 400 unidades originales alrededor del cráter estaban ocultos frente a una dirección de luz experimental. Se procesaron 2 135 vértices y se identificaron 640 como ocultos; al relacionar las posiciones con el paquete de Adventure a escala 2x, se aplicó la reducción de color a 901 entradas de vértice. Es una aproximación por vértice basada en rayos de Blender, no un horneado de texturas ni una sombra dinámica.

Una copia del paquete se cargó en Adventure y una repetición de vuelo siguió ejecutándose. En el fotograma comparado el efecto visible es pequeño y localizado, principalmente en un edificio; no se ha demostrado una mejora general ni suficiente detalle para sombras finas sobre terreno. La copia no se instaló. La comparación está en `work/blender-sombras/comparacion.png` y el paquete experimental en `work/blender-sombras/earth-blender-shadow.unk`.
