# Dragon Ball Z Budokai Tenkaichi 3: Reforged

Proyecto fan basado en Tenkaichi3Decomp que busca ampliar el modo historia de **Dragon Ball Z: Budokai Tenkaichi 3** con exploración, escenas y combates más fieles a la historia. No está afiliado ni cuenta con el respaldo de los titulares de los derechos.

**Prioridad actual:** construir y mejorar la jugabilidad del modo historia, empezando por la llegada de Raditz y la saga saiyajin. El alcance futuro incluye Dragon Ball clásico, Z, GT y Super.

Actualizado: 11 de noviembre de 2026.

## Estado actual

El prototipo de la misión de Raditz sigue esta secuencia:

**Confirmar la misión → explorar como Raditz → combate de Piccolo contra Raditz → regresar como Raditz → combate de Raditz contra Goku en Kame House → regresar como Goku → encuentro final y pelea nativa de BT3.**

La exploración usa el mapa de Dragon Adventure de BT2, convertido a BT3 y escalado al doble de su tamaño original. Raditz aparece sobre el cráter de su cápsula, en la zona identificada en el mapa. Los personajes de misión se desbloquean en orden: Piccolo, Goku y Raditz. Los encuentros bloqueados no aparecen en el mapa. Los encuentros de Piccolo y Kame House tienen combates y progreso guardado. El marcador final conecta con la pelea nativa. El usuario confirmó el recorrido completo, los desbloqueos, los resultados, el guardado y el regreso al mapa.

Adventure se abre desde el despachador de la misión antes de `Battle_Main` y tiene un ciclo de actualización y dibujo propio. En la versión compatible, el cargador de `src/adventure/adventure_loader.cpp` prepara y descarga los recursos del personaje jugable, el personaje de misión y, después de Kame House, del acompañante, sin ejecutar el trabajo inicial del cargador de combate. Las peleas siguen usando su carga original. Se reutilizan servicios y recursos internos de BT3 para archivos, guion, modelos, animaciones, sonido, escenario, cámara y efectos; por ello Adventure todavía depende del motor y de la estructura de esta versión del ejecutable. Si no puede instalarse el trabajo propio, existe una ruta de compatibilidad con el cargador nativo. Al salir se restaura la configuración de la misión. Véase [cargador propio de Adventure](docs/research/cargador-adventure.md).

### Columnas de misión

Los encuentros disponibles se señalan mediante columnas rojas. Al controlar a Raditz se muestran con el rastreador activado; al controlar a Goku se muestran directamente. Cuadrado confirma el destino más cercano dentro de una zona horizontal amplia, a cualquier altura; el personaje se aproxima volando y la pantalla se oscurece antes de la transición. Los modelos de personajes existentes permanecen visibles. Las columnas usan geometría 3D dibujada mediante funciones del motor; todavía conviene verificar su apariencia y oclusión en distintas situaciones.

Los archivos locales `mission-column.bin` y `mission-fade.bin` se pueden regenerar con `python tools/map-conversion/build_mission_effects.py external/local-only/DBZBT3R/piccolo-marker.bin external/local-only/DBZBT3R`.

### Selector de personajes

Mantén **Círculo** durante Adventure para mostrar el selector. Usa izquierda/derecha en la cruceta o el joystick izquierdo para elegir, y suelta Círculo para confirmar. Al inicio solo está disponible Raditz; después de Kame House se puede alternar entre Raditz y Goku acompañado por Piccolo. Si no cambias la selección, no se recarga la escena. Cambiar de personaje recarga Adventure conservando la posición y las victorias.

Las texturas locales se regeneran con `python tools/map-conversion/build_character_selector.py assets/ui/character-selector external/local-only/DBZBT3R/piccolo-marker.bin external/local-only/DBZBT3R`.

### Panel de administración

Durante Adventure, pulsa **F8** para abrir o cerrar el panel. Usa **arriba/abajo** para elegir un acto y **espacio** para marcarlo o desmarcarlo. Los cambios se guardan en el progreso de DBZBT3R y recargan brevemente Adventure conservando la posición. Marcar Kame House también completa Piccolo; desmarcar Piccolo desmarca Kame House, para mantener el orden de la historia. El panel funciona con el teclado mientras la ventana del juego tiene el foco.

### Funciones implementadas

- Lanzador que salta las presentaciones del juego y abre directamente el menú configurado.
- Mapa de la Tierra de BT2 navegable, con versiones locales para Blender y glTF.
- Raditz como personaje controlable en el tramo de exploración y punto de aparición sobre el cráter de su cápsula.
- Cámara de seguimiento y cambio de orientación durante el vuelo.
- Piccolo con capa acompaña a Goku durante el tramo final e imita su vuelo y efectos, con la separación ajustada.
- Minimapa de la Tierra con fondo transparente, flecha del personaje y puntos rojos para los encuentros disponibles. Con Raditz, los puntos se muestran al activar el rastreador.
- Raditz puede activar el rastreador con Triángulo: muestra el filtro, el marco y el objetivo disponible al enfocarlo.
- Tres velocidades: movimiento normal; movimiento rápido con dirección y botón X; y turbo mientras se mantiene L2. U ya no activa el turbo.
- Animaciones nativas para el vuelo rápido, el turbo y subir o bajar; impulso inicial, líneas blancas y sonido de inicio para el vuelo rápido. El turbo usa sus propios efectos.
- Malla de colisión inicial generada a partir de la geometría visible de la Tierra, consultada para el suelo, el movimiento y la cámara. El mar sin geometría usa un nivel provisional.
- Los encuentros disponibles se señalan con columnas 3D. Los personajes visibles en Adventure usan por ahora posturas y animaciones del juego.
- Al acercarse a los personajes y confirmar se pueden iniciar los combates de Piccolo contra Raditz y, después de ganar con Piccolo, de Raditz contra Goku en Kame House. Las victorias se guardan. Piccolo y Kame House pueden repetirse al controlar a Raditz; tras ganar en Kame House se vuelve como Goku y ambos encuentros anteriores dejan de estar disponibles mientras se controla al equipo de Goku.
- Raditz permite confirmar el encuentro final y pasar a la pelea nativa. Se desbloquea después de ganar el combate de Kame House.
- Desactivación del desenfoque de distancia heredado por el mapa y de partículas de clima de la plantilla que seguían a la cámara.

### Qué se ha comprobado y qué falta probar

El usuario consideró completados los objetivos anteriores de Adventure y del rastreador. Posteriormente se sustituyeron los iconos por personajes en el mundo y se reactivó el rastreador para Raditz.

El usuario confirmó el recorrido anterior con indicadores, los resultados, el guardado y el regreso al mapa. La nueva presentación con personajes requiere una prueba completa en juego; las transiciones se comprobaron mediante llegadas forzadas en pruebas aisladas.

## Objetivos y misiones

Los sistemas compartidos, su estado y sus pruebas pendientes están en [Objetivos generales de Adventure](docs/adventure/objetivos.md). La ruta progresiva, los combates y las cinemáticas previstas de cada episodio se mantienen por separado; el primer documento es [Misión de Raditz](historia/Dragon%20Ball%20Z/mision-raditz.md).

Las cinemáticas anteriores se retiraron del flujo del mod. Las escenas futuras figuran como objetivos en el documento de la misión correspondiente.

## Línea de tiempo

1. Se definió la idea de ampliar la historia de BT3 con cinemáticas, exploración y combates adicionales.
2. Se reprodujo un MP4 de la llegada de Raditz antes de la pelea y se añadió la opción de omitirlo con Enter.
3. Se eligió el mapa de Dragon Adventure de BT2; se extrajo, convirtió y preparó para inspección en Blender.
4. Se integró una primera prueba de vuelo en la misión y se comprobó el flujo de mapa a pelea nativa.
5. Se organizó el proyecto para trabajar desde el repositorio y se documentaron la arquitectura de BT2 y sus límites de lectura.
6. Se adelantó el ciclo de Adventure para ejecutarlo antes de `Battle_Init`, con carga y descarga temporales de los recursos necesarios.
7. Se ajustaron la cámara, el giro, el vuelo y las animaciones. Se añadieron tres velocidades, impulso, líneas y sonido de inicio.
8. Se corrigieron el desenfoque de distancia y las partículas de clima heredadas; se creó una primera colisión basada en la geometría del mapa.
9. Se sustituyó el marcador C por un icono de Raditz, se añadió confirmación al destino y se ajustó su comportamiento cuando queda detrás de la cámara.
10. Se cambió el personaje controlable a Raditz y se ubicó su aparición sobre el cráter señalado.
11. Se añadieron los encuentros de Piccolo y Kame House, sus cinemáticas y el guardado de victorias; después de Kame House se vuelve al mapa como Goku.
12. Se separó el ciclo de Adventure de la ejecución de la pelea y se implementó un cargador propio para los recursos de un solo personaje.
13. El usuario completó y verificó la ruta de Raditz, los desbloqueos, el guardado y las transiciones entre peleas y mapa.
14. Se sustituyeron los iconos flotantes por personajes de misión en el mundo y se recuperó el rastreador para Raditz. Esta versión necesita validación completa durante el juego.

## Referencias de diseño

- Dragon Ball Z: Budokai 3.
- Dragon Ball Z: Infinite World.
- Dragon Ball Z: Budokai Tenkaichi 2.
- Dragon Ball: Raging Blast 2.
- Dragon Ball FighterZ.
- Dragon Ball: Sparking! ZERO.
- Dragon Ball Xenoverse 2, como referencia para el rastreador.

El informe [Dragon Adventure de BT2: arquitectura aprovechable](docs/research/BT2-Dragon-Adventure-arquitectura.md) documenta el flujo que pudo estudiarse y las partes del código original que seguían sin ser legibles en la copia examinada. La [investigación de cámara y vuelo](docs/research/BT2-Dragon-Adventure-camera-flight.md) es una referencia histórica, no una descripción actualizada de todos los controles del prototipo. La [investigación del minimapa](docs/research/minimapa-bt2.md) documenta la conversión entre coordenadas del mundo y píxeles de la imagen y lo que falta para incorporarlo.

## Estructura del repositorio

- `src/adventure/`: entrada de misión, ciclo de Adventure, controles, colisión y direcciones de funciones utilizadas por el prototipo.
- `scripts/build-dbzbt3r.cmd`: compilación de la DLL con MSVC.
- `assets/maps/earth/`: documentación y recursos locales del mapa de la Tierra.
- `assets/ui/`: imagen fuente del minimapa, marcadores y referencias gráficas del rastreador. El marco final del rastreador se renderiza como una textura transparente de 512×448, dividida en celdas nativas de 64×64.
- `tools/map-conversion/`: herramientas de conversión, exportación y generación de colisión y marcador.
- `docs/`: manual de traspaso e investigaciones. El manual conserva pasos de una fase anterior; contrástalo con este estado y con el código antes de aplicarlo.
- `external/local-only/`: instalación y recursos locales excluidos de Git.

## Compilar y ejecutar

La versión usada para el prototipo es **Tenkaichi3Decomp Windows x64 v0.1.39.6**. Las direcciones internas del código pueden no funcionar con otras versiones.

Instala Visual Studio Build Tools 2022 con **Desarrollo para el escritorio con C++** y ejecuta:

```cmd
scripts\build-dbzbt3r.cmd
```

La DLL se genera en `build/DBZBT3R.dll` y, si existe el paquete local, se copia a `external/local-only/DBZBT3R/`. En esta computadora se inicia mediante `external/local-only/DBZBT3R/DBZBT3R.cmd`.

El progreso de este encuentro se guarda por separado de la partida original en `external/local-only/DBZBT3R/saves/dbzbt3r-progress.ini`. Se registran por separado las victorias de Piccolo y de Kame House; una derrota o salida sin victoria no completa el encuentro correspondiente.

Además de la instalación local de Tenkaichi3Decomp, el paquete necesita `earth-adventure-map.unk`, `earth-collision.bin`, `raditz-marker.bin`, `piccolo-marker.bin`, `goku-marker.bin`, `scouter-triangles.bin`, `scouter-filter.bin`, las texturas de las celdas ocupadas de la cuadrícula 8×7 (índices del 00 al 55), generadas desde la imagen 512×448. Las instrucciones para generar la colisión y el marcador están en [tools/map-conversion/README.md](tools/map-conversion/README.md). Un clon del repositorio no incluye por sí solo todos los archivos necesarios para jugar.

## Recursos locales y Git

Las ISO, el video, los ejecutables, las partidas, los mapas extraídos y los binarios generados permanecen locales y están excluidos mediante `.gitignore`. La carpeta `external/local-only/` contiene la instalación local y referencias de BT2; no se publica en GitHub. La imagen fuente del marcador y las herramientas sí forman parte del repositorio.

Las posiciones y la evidencia de los marcadores se describen en [marcadores-saga-saiyajin-bt2.md](docs/research/marcadores-saga-saiyajin-bt2.md).


Los encuentros de Piccolo y Kame House pueden repetirse en sus ubicaciones al controlar a Raditz, siempre que estén desbloqueados. Ni Piccolo ni Kame House se pueden iniciar mientras se controla a Goku con Piccolo. El límite de tiempo es de 40 segundos para Piccolo y 90 segundos para Kame House. Tras agotar el tiempo, el juego nativo determina el resultado.
