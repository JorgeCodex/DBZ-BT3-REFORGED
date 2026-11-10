# Dragon Ball Z Budokai Tenkaichi 3: Reforged

Repositorio del código fuente y la documentación del proyecto fan **Dragon Ball Z Budokai Tenkaichi 3: Reforged**. No está afiliado ni cuenta con el respaldo de los titulares de los derechos.

## Estructura

- `README.md`: estado actual, línea de tiempo, objetivos y notas de diseño de la Saga Saiyajin.
- `src/raditz-flight/`: código C++ del prototipo actual de vuelo de Raditz.
- `scripts/build-raditz-flight.cmd`: compila la DLL del prototipo usando MSVC.
- `assets/maps/earth/`: instrucciones y nombres de los recursos de mapa usados localmente; sus archivos derivados no se publican.
- `tools/map-conversion/`: contiene el código de conversión y exportación del mapa.
- `assets/README.md`: indica cómo manejar otros archivos locales del juego y recursos.
- `external/local-only/`: instalación local de Tenkaichi3Decomp v0.1.39.6, paquete ejecutable de Raditz y archivos locales de BT2. Esta carpeta está ignorada por Git.

## Compilación

Instala Visual Studio Build Tools 2022 con la carga de trabajo **Desarrollo para el escritorio con C++** y ejecuta `scripts/build-raditz-flight.cmd`. El resultado se guarda en `build/RaditzIntro.dll`; si existe el paquete local, el script también copia la DLL nueva a `external/local-only/Raditz-Vuelo/` para que el lanzador pruebe esa compilación.

El código se probó con Tenkaichi3Decomp para Windows x64 v0.1.39.6; el registro de la instalación confirma esa versión. Las direcciones internas pueden no servir con otras versiones. Los mapas extraídos y sus conversiones Blender/glTF se conservan localmente, pero no se incluyen en el repositorio público. La instalación decomp, la ISO, el video y el paquete binario Raditz están solo en `external/local-only/` y `.gitignore` evita que se suban a GitHub.

## Ejecutar la prueba local

En esta computadora, ejecuta `external/local-only/Raditz-Vuelo/Jugar Raditz - Vuelo.cmd`. El lanzador usa los datos locales de `external/local-only/Tenkaichi3Decomp-v0.1.39.6/gamedata`; el paquete Raditz contiene la DLL, `raditz-earth.unk` y `radditz.mp4`. No se debe esperar que funcione desde un clon de GitHub sin instalar los archivos requeridos en `external/local-only/`.

## Prototipo actual

La prueba reproduce un video de Raditz, carga un mapa de vuelo de la Tierra convertido y después llega a la misión nativa de Raditz de Budokai Tenkaichi 3. El mapa todavía no tiene colisiones adecuadas y el vuelo, el punto de aparición y algunos aspectos visuales necesitan correcciones. El resto de este README documenta el estado y el flujo planeado de la Saga Saiyajin.

## Código y recursos

Los archivos de mapa derivados de Budokai Tenkaichi 2 se mantienen en la copia local y están excluidos de Git. Se publican el código y las herramientas para que cada colaborador genere los recursos usando sus propios archivos de juego. No subas imágenes ISO, videos, partidas guardadas ni ejecutables compilados.


---

DRAGON BALL Z BUDOKAI TENKAICHI 3: REFORGED
BITÁCORA DEL ESTADO ACTUAL, LINEA DE TIEMPO Y NOTAS
Actualizado: 9 de noviembre de 2026

============================================================
1. PROPÓSITO DE ESTE DOCUMENTO
============================================================

Este documento registra el trabajo construido hasta ahora para el mod, el orden en que se fueron alcanzando los hitos y las notas de diseño para la Saga Saiyajin. Distingue entre la prueba que existe actualmente, sus fallas conocidas y el flujo que se quiere construir después.

El alcance global del proyecto comprende Dragon Ball clásico, Dragon Ball Z, Dragon Ball GT y Dragon Ball Super. El desarrollo actual se concentra en la jugabilidad del modo historia, empezando por la Saga Saiyajin y el episodio de Raditz. Namek también está incluido; su mapa se buscará y trabajará en una etapa posterior.

Juegos elegidos como referencias de diseño:
- Dragon Ball Z: Budokai 3
- Dragon Ball Z: Infinite World
- Dragon Ball Z: Budokai Tenkaichi 2
- Dragon Ball: Raging Blast 2
- Dragon Ball FighterZ
- Dragon Ball: Sparking! ZERO
- Shin Budokai, específicamente como referencia a consultar para el rastreador

Las referencias sirven para estudiar ideas de exploración, navegación, presentacion y combate. No implican copiar recursos ni sistemás completos.

============================================================
2. LINEA DE TIEMPO DEL PROYECTO
============================================================

La secuencia siguiente recoge los hitos en el orden en que ocurrieron. No se asignan fechas individuales porque no se conservaron fechas verificables para cada paso.

Hito 1 - Definicion de la idea
Se planteo ampliar una misión de la historia de Budokai Tenkaichi 3 para insertar escenas cinematograficas y tramos de exploración entre los momentos narrativos y los combates.

Hito 2 - Primera prueba con una cinemática
Se uso un fragmento del anime, específicamente la llega de radditz a la tierra para comprobar si podia reproducirse antes del combate nativo de la misión. Se comprobo que la cinemática se reproduce al iniciar la misión y que Enter permite saltarla.

Hito 3 - Busqueda de una base para el escenario
Se compararon las posibilidades de usar escenarios de Budokai 3 y Budokai Tenkaichi 2. Se eligio el mapa de aventura de la Tierra de Budokai Tenkaichi 2 como base técnica para probar el vuelo dentro del flujo de Budokai Tenkaichi 3.

Hito 4 - Extraccion del mapa de Budokai Tenkaichi 2
Se localizo el escenario de la Tierra dentro de los datos de la ISO de Budokai Tenkaichi 2. Se extrajeron sus datos y se preparo una conversión para poder inspeccionarlo y usarlo en la prueba de Budokai Tenkaichi 3.

Hito 5 - Inspección del escenario en Blender
Se convirtió el escenario a formatos visibles en herramientas de modelado 3D. Se preparo una versión Blender para inspeccion visual y una versión glTF. Al alejar la vista en Blender, la malla se veía seleccionada en naranja y el encuadre/clipping dificultaba verla; se ajustó el archivo de inspeccion para facilitar la vista general y se explicaron controles de visualizacion.

Hito 6 - Integracion del escenario en la misión de Raditz
Se integro el mapa de vuelo en el recorrido de la misión de Raditz, en sustitucion del flujo anterior de cinemática seguida directamente por la pelea. Se conservaron los datos del escenario de combate original para que la misión pudiera llegar al combate nativo.

Hito 7 - Prueba del flujo de cinemática, mapa y combate
Se comprobo el flujo actual:
escena de entrada de Raditz -> mapa navegable -> misión nativa de Budokai Tenkaichi 3.
La prueba permitió confirmar que el escenario puede cargarse en el recorrido de una misión y que luego se puede alcanzar el combate nativo. El usuario confirmo que el mapa no trae colisiónes utilizables para el movimiento deseado, pero considero que la prueba demuestra que el objetivo general es posible.

Estado al cierre de esta bitacora
Hay una prueba funcional de una cinemática inicial, un tramo de vuelo en el mapa adaptado de Budokai Tenkaichi 2 y la entrada posterior a la misión/pelea nativa de Budokai Tenkaichi 3 contra Raditz. La secuencia narrativa ampliada descrita en las notas aún no esta construida.

============================================================
3. LO QUE ESTA CONSTRUIDO Y COMPROBADO
============================================================

- Una cinemática MP4 de entrada para la misión de Raditz.
- La opción de saltar la cinemática con Enter.
- Carga de un escenario de vuelo derivado del mapa de aventura de la Tierra de Budokai Tenkaichi 2.
- Un tramo de movimiento navegable dentro de ese escenario.
- Una transición desde el tramo de mapa hacia el contenido nativo de Budokai Tenkaichi 3 para Raditz.
- Una copia de trabajo del mod y sus archivos de compilacion en la carpeta de salida del proyecto.
- Una versión del mapa preparada para inspeccion en Blender y otra versión glTF.

La prueba actual demuestra integración basica de cinemática, escenario de vuelo y combate nativo. No equivale a un mundo abierto completo ni a la secuencia narrativa final de la Saga Saiyajin.

============================================================
4. FALLAS Y TRABAJO TECNICO PENDIENTE
============================================================

1. El mundo no tiene colisión funcional.
El escenario no proporciona las colisiónes necesarias para el recorrido. Hace falta crear o adaptar colisiónes para el terreno y los obstáculos.

2. Desenfoque de movimiento / visualizacion durante el movimiento rápido.
Cuando el jugador se desplaza muy rápido, el mapa se ve mal o borroso. Hay que identificar si el origen esta en el efecto de movimiento, la cámara, la carga/renderizado del mapa u otra parte del juego.

3. Control del personaje al voltar parcialmente bugeado.
al moverse en multiples direcciones el personaje no respeta ciertas direcciones en especifico, aúnque vuela en esa dirección el personaje no se inclina hacia ahi, el frente la mayoria del tiempo termina mirando hacia otro lado. 

4. El personaje controlado debe ser Raditz.
La prueba debe reemplazar a Goku por Raditz durante el tramo de exploración correspondiente. Esto incluye comprobar modelo, animacion, posicion y control.

5. La posicion inicial (spawn) es incorrecta.
El punto de aparición debe colocarse junto a la cápsula de Raditz en el mapa, segun el flujo narrativo previsto.

6. El indicador C es provisiónal.
Debe sustituirse por un indicador más profesional y coherente con la interfaz del juego.

7. Mejorar el sistema de vuelo estudiando Budokai Tenkaichi 2.
Descompilar o analizar Budokai Tenkaichi 2 para entender como gestiona su mundo de aventura, el desplazamiento, la cámara y el vuelo. Aplicar lo aprendido a la implementación actual cuando sea viable.

8. Arreglar la colisión con el suelo.
El personaje debe interactuar correctamente con la superficie del mapa y no atravesarla ni quedar en una posicion incorrecta.

Estos puntos son problemás conocidos o tareas técnicas; aún no se consideran resueltos.

============================================================
============================================================
5. OBJETIVOS ORDENADOS POR IMPORTANCIA
============================================================

Marca cada objetivo cuando este completado. La lista esta ordenada de mayor a menor prioridad.

[ ] Corregir el vuelo y el control del personaje en el mapa.
[ ] Crear colisiones funcionales para el suelo y el terreno.
[ ] Colocar el punto de aparicion junto a la capsula de Raditz.
[ ] Controlar a Raditz durante el primer tramo de exploracion.
[ ] Analizar en Budokai Tenkaichi 2 como se gestionan el mundo y el vuelo.
[ ] Abrir el mapa navegable con minimapa despues de la escena de llegada de la capsula.
[ ] Investigar en Shin Budokai como funciona la vision del rastreador.
[ ] Implementar la activacion del rastreador y la deteccion del primer objetivo.
[ ] Agregar la cinematica de llegada al encuentro con Piccolo.
[ ] Agregar el combate de Raditz contra Piccolo.
[ ] Reproducir la cinematica posterior al combate y volver al mapa.
[ ] Bloquear el indicador de Goku hasta usar el rastreador y completar la pelea contra Piccolo.
[ ] Mostrar a Goku como objetivo del rastreador en direccion a Kame House.
[ ] Agregar la cinematica de llegada de Raditz a Kame House.
[ ] Agregar el combate de Raditz contra Goku en Kame House.
[ ] Reproducir la escena de Raditz marchandose y volver al mapa.
[ ] Colocar a Goku y Piccolo sobre Kame House para el tramo final de exploracion.
[ ] Mostrar el indicador de Raditz y cargar al llegar la pelea nativa de Budokai Tenkaichi 3.
[ ] Reproducir la cinematica de cierre con la muerte de Raditz y Piccolo llevandose a Gohan.
[ ] Corregir el desenfoque o deterioro visual durante el vuelo rapido.
[ ] Reemplazar el indicador C provisional por un marcador mas profesional.
[ ] Buscar en otro juego un mapa adecuado para incluir Namek en una etapa futura.
[ ] A largo plazo, reemplazar los modelos 3D de personajes, habilidades y escenarios.
6. NOTAS DE DISENO: SAGA SAIYAJIN
============================================================

Este apartado registra el flujo narrativo y jugable deseado para la llegada de Raditz. Los pasos siguientes son planes, no funciones que ya existan en la prueba actual.

A. Llegada de Raditz

1. Reproducir la escena de la cápsula de Raditz llegando al planeta.
2. Abrir el mapa navegable con minimapa.
3. Hacer aparecer al jugador justo en el lugar donde esta la cápsula de Raditz en el mapa.
4. Permitir presionar un boton para abrir la visión del rastreador.
5. Consultar Shin Budokai como referencia para investigar como implementar la visión/interfaz del rastreador.

B. Primera deteccion y encuentro con Piccolo

6. La primera deteccion del rastreador debe conducir a un nuevo combate: Raditz contra Piccolo.
7. Al llegar al punto de encuentro con Piccolo, reproducir una cinemática MP4.
8. Al terminar la cinemática, iniciar el combate de Raditz contra Piccolo.
9. Al terminar el combate, reproducir otra cinemática.
10. Al terminar esa segunda cinemática, regresar al mapa navegable.

C. Deteccion de Goku y Kame House

11. Despues de completar la pelea contra Piccolo, el jugador podrá abrir de nuevo el rastreador orientado hacia Kame House.
12. La deteccion debe revelar el indicador de Goku.
13. El indicador de Goku tiene una condicion estricta: no puede aparecer antes de usar el rastreador y completar la pelea contra Piccolo. El combate de Piccolo debe haberse completado para habilitar esa deteccion.
14. Reproducir la escena de Raditz llegando a Kame House.
15. Iniciar en ese escenario un combate de Raditz contra Goku.
16. Al terminar el combate, reproducir una escena de Raditz marchándose.

D. Trayecto final hasta el combate nativo de Budokai Tenkaichi 3

17. Despues de la escena de Raditz marchándose, volver al mapa navegable.
18. En este tramo, controlar a Goku y tener a Piccolo a su lado.
19. Colocar al grupo justo encima de Kame House en el mapa.
20. Mostrar un indicador de Raditz.
21. Al llegar al indicador, cargar por fin la pelea nativa de Budokai Tenkaichi 3 contra Raditz.

E. Cierre del episodio

22. Despues de la muerte de Raditz, reproducir una cinemática que muestre su muerte y a Piccolo llevandose a Gohan.
23. Finalizar aquí el episodio de Raditz.

============================================================
7. REGLAS DE PROGRESIÓN QUE NO SE DEBEN PERDER
============================================================

- El indicador de Goku esta bloqueado al inicio.
- El indicador de Goku solo se habilita después de completar la pelea de Raditz contra Piccolo.
- El rastreador es el medio para descubrir o revelar el objetivo; la progresión no debe mostrar el indicador de Goku antes de usarlo.
- La escena en Kame House y su pelea ocurren antes del tramo final que conduce a la pelea nativa de Budokai Tenkaichi 3.
- La pelea nativa de Budokai Tenkaichi 3 contra Raditz se reserva para el ultimo encuentro de la secuencia planificada.
- La cinemática de cierre con la muerte de Raditz y Piccolo llevandose a Gohan ocurre después de esa pelea nativa.

============================================================
8. ORDEN DE PRIORIDADES SUGERIDO A PARTIR DE LAS NOTAS
============================================================

Prioridad 1 - Estabilizar la prueba existente:
arreglar el vuelo, el spawn, el personaje controlado y la interacción con el suelo.

Prioridad 2 - Hacer recorrible el mapa:
crear colisiónes de terreno y obstáculos, fijar límites y evaluar el aspecto visual durante el vuelo rápido.

Prioridad 3 - Sustituir el indicador provisiónal:
definir y probar un marcador legible y acorde al estilo de la interfaz.

Prioridad 4 - Estudiar el sistema de aventura de Budokai Tenkaichi 2 y la visión de rastreador de Shin Budokai:
documentar comportamiento, controles, interfaz, orientación y condiciones de deteccion antes de implementarlos.

Prioridad 5 - Construir la progresión por estados:
cápsula -> primer rastreo -> Piccolo -> rastreo hacia Kame House -> Goku -> escena y combate en Kame House -> Raditz -> combate nativo -> cierre.

Prioridad 6 - Integrar las escenas y combates uno a uno:
probar cada transición antes de conectar toda la secuencia.

============================================================
9. ALCANCE FUTURO
============================================================

La prioridad actual es desarrollar la jugabilidad del modo historia, empezando por la Saga Saiyajin y el episodio de Raditz. El alcance futuro incluye Dragon Ball clásico, Dragon Ball Z, Dragon Ball GT y Dragon Ball Super. Namek se trabajará cuando se encuentre un mapa adecuado en otro juego y se compruebe que puede adaptarse.

============================================================
FIN DEL DOCUMENTO
============================================================












## Dependencias locales excluidas de Git

La ISO de Budokai Tenkaichi 2 y la copia de trabajo de BT2-Recomp se guardan localmente en `external/local-only/`. Esa carpeta está excluida por `.gitignore`: los archivos quedan disponibles en esta computadora, pero no se incluirán al subir el repositorio. El proyecto BT2-Recomp es una recompilación de código abierto; consulta su licencia dentro de esa carpeta y su repositorio de origen antes de redistribuir copias.



