# Dragon Ball Z Budokai Tenkaichi 3: Reforged

Proyecto fan basado en Tenkaichi3Decomp que busca ampliar el modo historia de **Dragon Ball Z: Budokai Tenkaichi 3** con exploración, escenas y combates más fieles a la historia. No está afiliado ni cuenta con el respaldo de los titulares de los derechos.

**Prioridad actual:** construir y mejorar la jugabilidad del modo historia, empezando por la llegada de Raditz y la saga saiyajin. El alcance futuro incluye Dragon Ball clásico, Z, GT y Super.

Actualizado: 10 de noviembre de 2026.

## Estado actual

El prototipo de la misión de Raditz sigue esta secuencia:

**Confirmar la misión → reproducir u omitir el MP4 de llegada → explorar como Raditz → confirmar el encuentro con Piccolo → controlar a Piccolo contra Raditz → volver al mapa como Raditz.**

La exploración usa el mapa de Dragon Adventure de BT2, convertido a BT3 y escalado al doble de su tamaño original. Raditz aparece sobre el cráter de su cápsula, en la zona identificada en el mapa. La vista actual muestra simultáneamente los iconos de Piccolo, Goku y Raditz en los puntos investigados de BT2. El encuentro de Piccolo ya se puede disputar; las demás peleas y la secuencia completa de la saga siguen pendientes.

Adventure se abre desde el despachador de la misión antes de `Battle_Main` y tiene un ciclo de actualización y dibujo propio. En la versión compatible, el cargador de `src/adventure/adventure_loader.cpp` reproduce la preparación necesaria para un solo personaje y su descarga, sin ejecutar el trabajo inicial del cargador de combate. Las peleas siguen usando su carga original. Se reutilizan servicios y recursos internos de BT3 para archivos, guion, modelos, animaciones, sonido, escenario, cámara y efectos; por ello Adventure todavía depende del motor y de la estructura de esta versión del ejecutable. Si no puede instalarse el trabajo propio, existe una ruta de compatibilidad con el cargador nativo. Al salir se restaura la configuración de la misión. Véase [cargador propio de Adventure](docs/research/cargador-adventure.md).

### Funciones implementadas

- Cinemática MP4 de llegada de Raditz; Enter permite omitirla.
- Lanzador que salta las presentaciones del juego y abre directamente el menú configurado.
- Mapa de la Tierra de BT2 navegable, con versiones locales para Blender y glTF.
- Raditz como personaje controlable en el tramo de exploración y punto de aparición sobre el cráter de su cápsula.
- Cámara de seguimiento y cambio de orientación durante el vuelo.
- Tres velocidades: movimiento normal; movimiento rápido con dirección y botón X; y turbo mientras se mantiene L2. U ya no activa el turbo.
- Animaciones nativas para el vuelo rápido, el turbo y subir o bajar; impulso inicial, líneas blancas y sonido de inicio para el vuelo rápido. El turbo usa sus propios efectos.
- Malla de colisión inicial generada a partir de la geometría visible de la Tierra, consultada para el suelo, el movimiento y la cámara. El mar sin geometría usa un nivel provisional.
- Iconos de Piccolo, Goku y Raditz en las posiciones de los marcadores de BT2, con posición suavizada. Los iconos se ocultan cuando el destino está detrás de la cámara.
- Al llegar al marcador de Piccolo, Enter o START inicia un combate de Piccolo controlable contra Raditz. Al terminar, la aventura vuelve al mapa como Raditz; una victoria se guarda para no repetirla. La pelea final nativa todavía no está conectada a la nueva progresión.
- Desactivación del desenfoque de distancia heredado por el mapa y de partículas de clima de la plantilla que seguían a la cámara.

### Qué se ha comprobado y qué falta probar

Se ha visto a Raditz aparecer sobre el cráter y volar en Adventure. Se comprobaron las tres velocidades, las líneas de movimiento y el cambio del marcador mediante pruebas aisladas; la sensación de vuelo, el sonido y la estabilidad del icono deben seguir evaluándose al jugar.

Una prueba con Raditz en Adventure y el punto de aparición anterior llegó a la escena de combate nativa con Goku. **Aún queda repetir el recorrido completo desde el nuevo punto de aparición**, confirmar el destino y comprobar la pelea y sus efectos. La colisión existe, pero necesita una revisión manual del terreno, los edificios, la vegetación, los huecos y el agua. El desenfoque de distancia se corrigió; la posible pérdida de claridad durante el movimiento rápido sigue por evaluar por separado.

## Objetivos

Las casillas marcadas indican que existe una primera implementación o investigación concreta. Las tareas de pulido y validación de esa implementación aparecen por separado para que puedan tacharse cuando se completen.

### Base de Adventure y estabilidad

- [x] Reproducir una cinemática de llegada de Raditz antes de la exploración y permitir omitirla con Enter.
- [x] Extraer y convertir el mapa de la Tierra de BT2 para usarlo en el prototipo.
- [x] Preparar versiones del mapa para inspeccionarlo en Blender y glTF.
- [x] Saltar las presentaciones y llegar más rápido al menú del juego desde el lanzador.
- [x] Iniciar el ciclo de Adventure antes de `Battle_Init` y devolver después el control al inicio de la pelea.
- [x] Controlar a Raditz durante el primer tramo de exploración.
- [x] Colocar el punto de aparición sobre el cráter de la cápsula de Raditz.
- [x] Implementar movimiento y orientación del personaje en el mapa.
- [x] Implementar las tres velocidades de vuelo, sus animaciones y el impulso inicial del vuelo rápido.
- [x] Añadir animaciones al subir y bajar durante la exploración.
- [x] Añadir líneas de movimiento y sonido de inicio al vuelo rápido.
- [x] Crear una primera malla de colisión del suelo y los objetos a partir del mapa visible.
- [x] Reemplazar el indicador C por un icono de Raditz y suavizar su movimiento.
- [x] Probar la confirmación de un destino y su conexión con la misión nativa de Raditz en la versión anterior.
- [x] Localizar en BT2 y mostrar los puntos de Piccolo, Kame House y el encuentro final con Raditz.
- [ ] Integrar de nuevo la confirmación y la pelea nativa en la progresión definitiva de los marcadores.
- [x] Corregir el desenfoque de distancia heredado por el mapa de Adventure.
- [x] Investigar y documentar el flujo de historia, mapa y eventos de BT2 con las fuentes disponibles.
- [ ] Repetir la ruta completa desde el nuevo punto de aparición hasta la pelea y comprobar la restauración de la misión.
- [ ] Probar y ajustar el vuelo, el giro, la cámara, las animaciones y los sonidos en condiciones normales de juego.
- [ ] Revisar y corregir las colisiones del terreno, edificios, vegetación, agua y límites del mapa.
- [ ] Evaluar y corregir el deterioro visual que aún pueda ocurrir durante el vuelo rápido.
- [ ] Profundizar en el código de BT2 para recuperar los detalles de vuelo, cámara, llegada a destinos y gestión del mundo que todavía no se conocen.
- [ ] Reducir la dependencia de recursos de combate y avanzar hacia un sistema de Adventure independiente.
- [ ] Añadir el minimapa de BT2 con la posición del personaje y los indicadores correspondientes.

### Saga saiyajin: episodio de Raditz

- [ ] Investigar la visión del rastreador de Shin Budokai como referencia.
- [ ] Implementar la activación del rastreador y la primera detección, dirigida al encuentro con Piccolo.
- [x] Añadir una cinemática antes del combate de Raditz contra Piccolo.
- [x] Añadir el combate de Piccolo controlable contra Raditz y volver al mapa como Raditz.
- [x] Guardar la victoria contra Raditz y permitir reintentar el encuentro si Piccolo pierde.
- [x] Reproducir una cinemática después de ganar ese combate y regresar al mapa navegable.
- [ ] Revelar el indicador de Goku mediante el rastreador solo después de completar la pelea contra Piccolo.
- [x] Dirigir a Raditz hacia Kame House y reproducir la cinemática antes del combate.
- [x] Añadir el combate de Raditz controlable contra Goku en el escenario de Kame House y guardar la victoria.
- [x] Regresar al mapa como Goku tras ganar en Kame House.
- [ ] Probar manualmente el combate de Kame House, su resultado y el regreso al mapa.
- [x] Reproducir la escena posterior a la victoria en Kame House y regresar al mapa.
- [ ] Comenzar el tramo final sobre Kame House con Goku controlable y Piccolo a su lado.
- [ ] Mostrar a Raditz como objetivo durante el tramo final y conectar ese encuentro con la pelea nativa de BT3.
- [ ] Reproducir la cinemática de la muerte de Raditz y de Piccolo llevándose a Gohan.
- [ ] Rehacer el combate final para incluir a Goku y Piccolo contra Raditz y la intervención de Gohan.

### Objetivos posteriores

- [ ] Desarrollar las demás sagas, empezando por el resto de Dragon Ball Z.
- [ ] Buscar y adaptar un mapa de Namek de otro juego.
- [ ] Ampliar el modo historia a Dragon Ball clásico, GT y Super.
- [ ] Sustituir progresivamente los fragmentos del anime por escenas creadas con el motor del juego.
- [ ] Rehacer los combates necesarios para representar mejor los acontecimientos de cada saga.
- [ ] A largo plazo, reemplazar modelos 3D de personajes, habilidades y escenarios.

## Diseño previsto: llegada de Raditz

1. Reproducir la llegada de la cápsula y abrir el mapa con minimapa; Raditz aparece junto al lugar de impacto.
2. El jugador activa el rastreador. La primera detección conduce a Piccolo, donde una cinemática da paso a un combate nuevo entre Raditz y Piccolo.
3. Tras el combate y otra cinemática, regresar al mapa. Al usar de nuevo el rastreador se revela a Goku y el trayecto hacia Kame House.
4. Reproducir la llegada a Kame House, disputar un combate nuevo entre Raditz y Goku y mostrar a Raditz marchándose.
5. Regresar al mapa sobre Kame House con Goku y Piccolo. El indicador de Raditz conduce al combate nativo de BT3, que más adelante se rehacerá para incluir la lucha conjunta y la intervención de Gohan.
6. Después de la muerte de Raditz, mostrar su muerte y a Piccolo llevándose a Gohan. Con eso termina el episodio.

**Regla de progresión:** el indicador de Goku no debe aparecer antes de que el jugador haya usado el rastreador y completado el combate contra Piccolo. Actualmente los tres indicadores permanecen visibles para pruebas, pero el combate de Kame House solo se puede activar después de ganar con Piccolo. Al ganar con Raditz, el progreso se guarda y se vuelve al mapa como Goku. El rastreador, Piccolo como acompañante y el combate final todavía están pendientes.

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
10. Se cambió el personaje controlable a Raditz y se ubicó su aparición sobre el cráter señalado. Está pendiente repetir desde ahí el recorrido completo hasta la pelea.

## Referencias de diseño

- Dragon Ball Z: Budokai 3.
- Dragon Ball Z: Infinite World.
- Dragon Ball Z: Budokai Tenkaichi 2.
- Dragon Ball: Raging Blast 2.
- Dragon Ball FighterZ.
- Dragon Ball: Sparking! ZERO.
- Shin Budokai, como referencia para el rastreador.

El informe [Dragon Adventure de BT2: arquitectura aprovechable](docs/research/BT2-Dragon-Adventure-arquitectura.md) documenta el flujo que pudo estudiarse y las partes del código original que seguían sin ser legibles en la copia examinada. La [investigación de cámara y vuelo](docs/research/BT2-Dragon-Adventure-camera-flight.md) es una referencia histórica, no una descripción actualizada de todos los controles del prototipo. La [investigación del minimapa](docs/research/minimapa-bt2.md) registra lo que falta para incorporarlo.

## Estructura del repositorio

- `src/adventure/`: cinemática, ciclo de Adventure, controles, colisión y direcciones de funciones utilizadas por el prototipo.
- `scripts/build-dbzbt3r.cmd`: compilación de la DLL con MSVC.
- `assets/maps/earth/`: documentación y recursos locales del mapa de la Tierra.
- `assets/ui/`: imágenes fuente de los marcadores de Piccolo, Goku y Raditz.
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

El progreso de este encuentro se guarda por separado de la partida original en `external/local-only/DBZBT3R/saves/dbzbt3r-progress.ini`. La victoria se registra solo cuando gana Piccolo; una derrota o una salida sin victoria no la completan.

Además de la instalación local de Tenkaichi3Decomp, el paquete necesita `earth-adventure-map.unk`, `earth-collision.bin`, `raditz-marker.bin`, `piccolo-marker.bin`, `goku-marker.bin` y, para la cinemática, `saiyan-pod-landing.mp4`. Las instrucciones para generar la colisión y el marcador están en [tools/map-conversion/README.md](tools/map-conversion/README.md). Un clon del repositorio no incluye por sí solo todos los archivos necesarios para jugar.

## Recursos locales y Git

Las ISO, el video, los ejecutables, las partidas, los mapas extraídos y los binarios generados permanecen locales y están excluidos mediante `.gitignore`. La carpeta `external/local-only/` contiene la instalación local y referencias de BT2; no se publica en GitHub. La imagen fuente del marcador y las herramientas sí forman parte del repositorio.

Las posiciones y la evidencia de los marcadores se describen en [marcadores-saga-saiyajin-bt2.md](docs/research/marcadores-saga-saiyajin-bt2.md).


### Cinemáticas locales de los encuentros

Los vídeos se guardan en `external/local-only/DBZBT3R/cinematics/` y no se suben a GitHub. Se pueden saltar con Enter. Para cada encuentro, el archivo terminado en `-0.mp4` se reproduce antes de la pelea y el que tenga el número positivo más alto se reproduce después de ganar. Los números intermedios quedan reservados para futuros eventos dentro del combate; actualmente no se reproducen.

- Piccolo: `RadditzvsPiccolo-0.mp4` y `RadditzvsPiccolo-1.mp4`.
- Kame House: `GokuvsRadditz-0.mp4` y `GokuvsRadditz-1.mp4`.

Si se pierde o abandona la pelea, no se reproduce la escena de victoria y se puede reintentar.

Los encuentros de Piccolo y Kame House pueden repetirse desde sus marcadores con Enter/Start, sin borrar las victorias guardadas. El límite de tiempo es de 40 segundos para Piccolo y 90 segundos para Kame House. Tras agotar el tiempo, el juego nativo determina el resultado.
