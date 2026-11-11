# Dragon Adventure de BT2: arquitectura aprovechable para Reforged

Fecha: 9 de noviembre de 2026.

## Alcance y resultado

Revisé la copia local de BT2-Recomp, sus herramientas y pruebas de historia, el contenido de escenas de la ISO PAL y los puntos de entrada del prototipo de Raditz. El objetivo es decidir qué sirve para darle a Reforged un modo de aventura propio. El análisis de cámara queda para otra etapa.

BT2 organiza la historia con un coordinador que abre el mapa, interpreta eventos, prepara un combate cuando corresponde y procesa el resultado. Reforged puede implementar esa separación con su propio código. El trabajo decisivo consiste en mover la entrada a la exploración antes de inicializar la batalla y darle al mapa un ciclo de carga, actualización y descarga bajo su control.

## Qué hay en BT2-Recomp

BT2-Recomp recompila el ejecutable de PS2 de la edición estadounidense `SLUS-21441` mediante PS2Recomp. Los cuerpos de las funciones originales de Dragon Adventure se generan localmente a partir del ejecutable. La copia revisada contiene el runtime, herramientas, configuración y documentación. La carpeta de código generado no está presente. Por tanto, la auditoría del código se limita a lo que está publicado o disponible localmente: herramientas, instrumentación y notas de ingeniería inversa, más los guiones y datos leídos de la ISO. No se reconstruyó ni ejecutó el ELF PAL en este análisis.

| Fuente | Qué se puede leer | Qué sirve para Reforged |
| --- | --- | --- |
| `docs/plans/ee/STORY_MODE_PLAN.md` y `WHOLE_GAME_COVERAGE_PLAN.md` | El despacho de historia, el orden de carga, el bucle del mapa, las consultas de destino y el inicio y fin de batalla. También las ramas de pueblos, tiendas y persecuciones. | El reparto de responsabilidades entre coordinador, exploración, eventos y batalla. |
| `tools/ps2recomp/story_scenes.py` | Python que lee ISO 9660 y AFS y decodifica contenedores `GSCF`, bloques `GSAC` e índices de datos `GSDT`. Reconoce varios comandos de transición. | La técnica para extraer mapas y auditar conexiones. La ruta del AFS, el límite de archivos y la tabla del ejecutable están fijados para la versión USA. El lector no ejecuta los guiones ni interpreta todos los opcodes. |
| `runtime/probe/game_starter.cpp` | C++ de diagnóstico. `startEpisode` establece episodio, parte y bandera. `reachMarker`, `reachedMarker` y `catchEnemy` sustituyen temporalmente respuestas de llegada. `nextScene` registra la transición y llama a la función original. | Los puntos donde observar eventos y un patrón para registrar y comprobar transiciones. Las respuestas forzadas pertenecen al banco de pruebas. |
| `tools/ps2recomp/story_sweep.py` y `docs/progress/VALIDATION.md` | Selección de casos, cobertura por episodios y pueblos, escenas visitadas y resultados de combate. | Un patrón para comprobar rutas y regresiones del coordinador de Reforged. |
| `runtime/`, `CMakeLists.txt` y `docs/guides/BUILDING.md` | Infraestructura de ejecución, entrada, audio y render, además de la generación del código EE. | Contexto para saber dónde vive el código original. El runtime atiende servicios de PS2 y los cuerpos del juego se generan aparte. |

Referencias: [plan de historia](https://github.com/oric212/BT2-Recomp/blob/master/docs/plans/ee/STORY_MODE_PLAN.md), [código del iniciador](https://github.com/oric212/BT2-Recomp/blob/master/runtime/probe/game_starter.cpp), [validación](https://github.com/oric212/BT2-Recomp/blob/master/docs/progress/VALIDATION.md), [guía de compilación](https://github.com/oric212/BT2-Recomp/blob/master/docs/guides/BUILDING.md).

## Cómo se separan aventura y combate en BT2

Las direcciones de esta sección pertenecen a la revisión estadounidense estudiada por BT2-Recomp. Sirven como referencias de investigación.

1. **El menú selecciona un episodio.** El programa principal lee el progreso y, con la bandera de historia `0x200`, llama al coordinador de historia `0x2A8A38`. La rama normal de combate tiene su propio punto de entrada, `0x103000`.
2. **El coordinador abre la escena de exploración.** La fase `0x2AC308` carga el guion, lo entrega a la máquina de scripts y prepara la escena en `0x2ACF90`. Las notas distinguen la actualización del mapa `0x2ACB20` del control de eventos `0x2AC1F8`.
3. **El mapa procesa destinos.** El bucle de mapa `0x2ABF30` consulta la salida mediante `0x2AD898`. Este callback consulta si se alcanzó un marcador y cuál fue mediante `0x20C870` y `0x20C8E8`. El identificador selecciona un bloque `2000 + marcador` o `3000 + marcador`, según el tipo de mapa. El comando 1002 de ese bloque indica la escena siguiente. La fase de mapa termina con un fundido cuando la ruta lo requiere.
4. **El destino decide la acción.** Una salida puede llevar a otra escena, a un pueblo o a la preparación del combate. Los pueblos tienen sus propios actores e interacciones. Una persecución consulta por separado si el objetivo fue alcanzado. Esto permite representar tipos de evento distintos con el mismo coordinador.
5. **El combate se inicia al recibir su configuración.** El comando 1010 prepara los datos de pelea y recompensas. La pantalla previa los pasa a las funciones de batalla, se ejecuta el combate y el coordinador procesa el resultado. Ese resultado decide si el episodio progresa o vuelve a la parte actual.

El proyecto registra que 200 partes y 73 destinos opcionales recorrieron el flujo de prueba. El barrido de noviembre llegó al final de las 200 peleas, con 70 victorias y 130 derrotas. En esos recorridos se fuerzan llegadas, capturas en persecución e interacciones y se responde a confirmaciones mediante el iniciador. La evidencia cubre el encadenamiento de las fases y el procesamiento de sus resultados. La conducción manual, la zona de activación y la secuencia de aterrizaje quedan por verificar. [Validación de BT2-Recomp](https://github.com/oric212/BT2-Recomp/blob/master/docs/progress/VALIDATION.md)

## Qué confirmé en la ISO PAL del proyecto

- `SYSTEM.CNF` declara `SLES_541.64` y `VMODE = PAL`. `ZS2EU_1.AFS` tiene 13 197 entradas y `ZS2EU_2.AFS` tiene 34 607. Los segmentos cargables que declara el ELF de arranque empiezan en `0x01B00000` y `0x01B52B80`. La tabla USA de episodios en `0x344F30` queda fuera de esos segmentos. Su ubicación PAL y el código que ejecuta la lógica de mapa siguen sin localizarse.
- Leí las entradas 33 a 653 del primer AFS con el decodificador de escenas, ajustando en memoria el archivo de origen. Las 621 tienen firma `GSCF` y sus bloques se pudieron recorrer. Usando grupos de tres como en la documentación de USA, 193 de los 207 primeros guiones tienen un marcador que apunta a la escena inmediatamente siguiente. Siete más tienen una transición de arranque a esa escena sin dicho marcador. En siete no encontré ninguno de esos enlaces. Encontré además 436 enlaces a 73 escenas opcionales y tres guiones que usan el comando de persecución 1013 para enlazar con la escena siguiente, en las entradas 123, 240 y 486. Son conteos estáticos de enlaces, sin ejecución PAL.
- En la escena 33, el marcador 14 lleva a la escena 34. Los marcadores 5, 7, 8, 11 y 21 apuntan respectivamente a 727, 733, 734, 664 y 665. El guion de 664 contiene el comando 1008 en el bloque de interacción, el mismo comando que las notas de USA asocian a la tienda.
- El identificador global 18204 de la escena 33 corresponde a la entrada 5007 del segundo AFS. Comparé los bytes de esa entrada con `assets/maps/earth/source-bt2-earth.unk` y coinciden. Es el paquete de 18 bloques que ya tiene Reforged.
- `tools/map-conversion/convert_earth.py` incorpora el bloque visual 0 de BT2, convierte su árbol de visibilidad, conserva un `MAPD` de la plantilla de BT3 con un campo modificado y deja vacío el bloque 6. El mapa convertido demuestra que podemos usar la geometría. El análisis no identifica un bloque de colisión de BT2 ni establece que los límites, puntos de aparición o colisiones originales se hayan trasladado.

La comprobación se hizo leyendo la ISO y comparando el paquete existente, sin ejecutar el juego ni extraer nuevos recursos al repositorio. Se puede repetir con las clases `Disc` y `Archive` del lector y las funciones `scene()` y `values()`. Para esta comprobación PAL se recorrieron índices del AFS directamente, sin usar `part_counts()` de USA.

## Qué falta recuperar del código original

| Parte | Estado | Decisión para la primera versión |
| --- | --- | --- |
| Creación, actualización y liberación de la escena de mapa | Se conocen el flujo y puntos de entrada USA, pero faltan los cuerpos de las rutinas y sus dependencias de memoria en la copia. | Crear un ciclo propio. Identificar en BT3 las funciones de carga y dibujo que puedan ejecutarse desde él. |
| Actor de exploración | Los guiones referencian modelos y las notas identifican objetos del mapa. No están reconstruidas aquí las estructuras completas, la creación del actor, la asociación con animaciones ni el orden de destrucción. | Guardar posición, orientación y animación como estado de aventura. Usar un objeto visual cuya carga y actualización estén bajo control del modo. |
| Vuelo, suelo y límites | Los recorridos automatizados evitan el trayecto real. Las estructuras de terreno, consultas de contacto y fórmulas de movimiento quedan sin resolver. | Implementar controles y límites provisionales con datos explícitos. Mantener pendiente la colisión fiel al terreno. |
| Llegada y aterrizaje | Se conoce el consumo del evento y el salto de guion. No se identificó la rutina que escribe el estado de llegada, su volumen exacto ni la animación original. | Separar proximidad, confirmación, transición y carga. Usar una transición sencilla de momento. |
| Máquina de guiones | Son legibles el formato y varios comandos. El significado de todos los opcodes, los tipos de argumentos y parte de las condiciones internas sigue abierto. | Definir datos propios de campaña, con condiciones y destinos explícitos. |
| Ramas opcionales | La estructura de pueblos, tienda y resultados está documentada. Faltan la mecánica real de persecución, la lógica que inicia varios combates de pueblo y los posibles objetos recogibles fuera de sus guiones. | Dejar sitio para acciones nuevas en el coordinador y posponer esas ramas hasta que exista la primera ruta completa. |

Si se genera el C++ del ejecutable compatible, los símbolos neutros por dirección, registros de CPU y accesos a memoria seguirán requiriendo interpretación. Una función que corre durante un barrido no equivale a una función con tipos y propósito recuperados. Si después queremos reproducir un comportamiento con más fidelidad, el trabajo acotado sería seguir quién escribe el indicador de llegada que leen las consultas de mapa, y registrar esa ruta con entrada real. BT2-Recomp ya documenta trazas de llamada y observación de memoria para su revisión USA. [Herramientas de diagnóstico](https://github.com/oric212/BT2-Recomp/blob/master/docs/guides/TOOLS.md)

## Aplicación a BT3 Reforged

El prototipo ya tiene un bucle manual de vuelo, pero lo inicia desde `RaditzBattleLoop()` una vez que existen ambos combatientes. `Explore()` obtiene al jugador y al oponente con `BtlChar_Get`, oculta el objeto rival y toma el objetivo de la posición inicial del segundo combatiente. Al salir, el parche recupera la arena original y llama a `Battle_Restart()`. El punto que hay que cambiar es el ciclo de vida del modo, no la fórmula de vuelo.

En la descompilación PS2 de BT3, el menú retorna al bucle principal y este llama a `Battle_Main`. `Battle_Init` carga los recursos de combate y llama a `BtlChar_AllocAll(2)`, además de iniciar otros subsistemas de pelea. Esto da un límite concreto: localizar la ruta equivalente en el port x64 v0.1.39.6 y ejecutar la aventura entre la confirmación de misión y la primera inicialización de combate. Las direcciones PS2 y los desplazamientos del ejecutable x64 se deben resolver por separado. [Bucle principal de BT3](https://github.com/z3xox/BT3-Decompiled/blob/main/docs/systems/boot_and_frame.md) · [Inicialización de combate](https://github.com/z3xox/BT3-Decompiled/blob/main/src/battle/battle.c)

### Secuencia de trabajo

1. **Aislar el punto de entrada.** Aprovechar que `PlayRaditzIntro()` ya identifica la confirmación de la misión. Conservar la selección de misión y encontrar el punto anterior a `Battle_Init` donde un coordinador pueda tomar el control. Comprobar qué recursos del menú siguen vivos en ese punto.
2. **Cargar el mapa como escena propia.** Hacer primero una escena mínima con entrada, actualización, dibujo y descarga. Reutilizar la geometría de la Tierra y las funciones generales de cuadros, mandos, sonido, fundido y dibujo que puedan inicializarse de forma independiente. Las rutinas actuales de escenario son candidatas sujetas a revisar sus dependencias de `gCommonRes`, de las colas y del estado global de batalla. Si un cargador requiere inicializar toda una pelea, aislar la carga de escenario o utilizar un adaptador de mapa propio.
3. **Añadir un actor y un destino.** Darle al modo la propiedad del actor, el punto de aparición y el objetivo. Usar coordenadas propias del mapa. La primera ruta puede llevar a Raditz desde la cápsula hasta un único destino de prueba, con vuelo básico y confirmación al entrar en su zona.
4. **Entregar la misión al combate.** Consumir el evento una sola vez, descargar el mapa y sus recursos y devolver el control al arranque normal de la misión nativa. Si falla la carga de aventura, liberar lo ya adquirido y continuar por el arranque normal. Dejar el retorno al mapa como fase siguiente, tras identificar el punto que entrega el resultado del combate.
5. **Ampliar la historia mediante datos.** Mantener la misión y sus fases fuera del estado temporal del mapa. Un destino puede reproducir una escena, iniciar un combate o cambiar de mapa. El indicador de Goku se habilitará tras completar el encuentro con Piccolo, con la condición de haber usado el rastreador según el diseño de Reforged.

### Criterio para dar por separada la aventura

La secuencia de Raditz debe poder reproducir u omitir la cinemática, cargar la Tierra y mover un actor con el sistema de combate todavía sin inicializar. Confirmar el destino debe cerrar la escena y arrancar la pelea nativa una sola vez. La traza del prototipo debería dejar claro el orden `cinemática → carga de aventura → destino → descarga de aventura → primera inicialización de combate`. Durante el mapa no debe necesitar `BtlChar_Get(1)`, un oponente oculto, un intercambio de arena en plena batalla ni `Battle_Restart()`. Una vista provisional es suficiente para comprobar esa separación.
