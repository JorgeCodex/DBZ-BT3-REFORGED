# Dragon Adventure de BT2: cámara, vuelo y destinos

Estado: análisis inicial. Esta nota separa los datos confirmados de lo que todavía requiere rastrear el ejecutable en tiempo de juego.

## Conclusión principal

Dragon Adventure no es una batalla de Tenkaichi 2 sin rival. Es un modo de historia con un bucle de mapa propio, entrada de controles propia, objetos de mapa y eventos que conducen a escenas, pueblos o combates. El prototipo actual de BT3, en cambio, entra en el bucle de batalla, oculta al oponente y simula el vuelo con los objetos y la cámara de una batalla. Por eso no conviene seguir ajustando esa cámara como si ya fuera la cámara del mapa de BT2.

## Controles y cámara documentados

El manual consultado es para la edición estadounidense de PS2. Describe estos controles del mapa; todavía hay que contrastarlos con la edición PAL SLES_541.64 del proyecto:

- Stick izquierdo: avanzar o retroceder.
- Stick derecho: rotar la cámara.
- L2: volar a supervelocidad.
- L1: sentir auras Ki.
- X: aterrizar en puntos de interés.
- START: abrir el menú de pausa.

Los eventos de historia aparecen como círculos rojos, otros lugares importantes como círculos amarillos y algunos lugares no tienen indicador. Al volar sobre un evento aparece una descripción; el jugador pulsa X para aterrizar y explorar el lugar. El manual también describe en el menú Screen tres tipos de cámara: media distancia (predeterminada), lejana y cercana. El texto no aclara si ese ajuste afecta Dragon Adventure o solo las cámaras de los combates, así que no debe tomarse como prueba de que el mapa tenga zoom con tres modos.

Fuente primaria: [manual estadounidense de Budokai Tenkaichi 2, controles y Dragon Adventure](https://www.videogamemanual.com/PS2/Dragon%20Ball%20Z-%20Budokai%20Tenkaichi%202%20%28USA%29.pdf), páginas impresas 3, 10 y 13.

## Lo que aporta la ISO PAL del proyecto

La ISO de trabajo es la edición europea SLES_541.64. Sus archivos `DATA/ZS2EU_1.AFS` y `DATA/ZS2EU_2.AFS` se pueden leer con el formato de archivo de BT2-Recomp. La primera escena del episodio 0 está en la entrada 33 de `ZS2EU_1.AFS`; el análisis de sus datos encontró:

- valor 18204 como recurso de mapa de la Tierra;
- los marcadores 5, 7, 8, 11, 14 y 21 llevan a los valores/escenas 727, 733, 734, 664, 34 y 665;
- el marcador 14 conduce a la escena 34.

Esto confirma en el disco europeo la relación entre datos de escena, mapa y destinos. No revela por sí solo la fórmula de cámara, la velocidad de vuelo ni la animación de aterrizaje.

## Arquitectura del mapa en BT2-Recomp

La documentación de Dragon Adventure de BT2-Recomp describe un bucle de historia que carga el mapa, actualiza el mapa una vez por cuadro y comprueba sus eventos. Cuando el personaje alcanza un marcador, el sistema identifica cuál fue, ejecuta el bloque correspondiente del guion de historia y realiza una transición con fundido a la escena indicada. Algunos destinos llevan a mapas pequeños con personajes y diálogos; algunos terminan en combates. Las persecuciones usan una condición separada de enemigo alcanzado, no el mismo evento que los marcadores normales.

La documentación indica que el bucle del mapa de historia (`0x2ABF30`) y la comprobación de salida/evento (`0x2AD898`) son distintos de la rutina de batalla. Esas direcciones pertenecen a la edición estadounidense que analiza BT2-Recomp y no deben copiarse directamente a la ISO PAL sin validar las diferencias.

Referencia de trabajo: `external/local-only/BT2-Recomp/docs/plans/ee/STORY_MODE_PLAN.md`; la carpeta local de BT2-Recomp está excluida de Git.

## Diferencias con el prototipo de BT3

En `src/adventure/flight.cpp`, `Explore()` conserva los objetos de los dos combatientes, desactiva el rival para que no se dibuje y ejecuta un bucle de vuelo propio dentro del ciclo de batalla. La posición del personaje se escribe con `BtlChar_Place`; la vista se configura a través de estructuras de personaje y funciones de cámara de combate como `BtlCam_TraceStage`.

La cámara actual es una aproximación creada para la prueba:

- apunta a la posición del jugador con un desplazamiento vertical;
- calcula la posición de la cámara detrás del personaje usando el yaw de cámara;
- usa tres distancias y una interpolación de 0.1 por cuadro;
- rota con el stick/dirección de giro y deja que la cámara siga gradualmente al personaje;
- SELECT cambia los tres modos de distancia.

El movimiento actual también es provisional: 8 unidades por cuadro en velocidad normal, 22 con turbo, 6 en vertical y suavizado de velocidad de 0.25. Estos son valores del mod de BT3, no medidas de BT2. En el destino, el mod espera una distancia menor que 65, reproduce la animación 0 de la estructura de batalla y espera 90 cuadros antes de salir. Eso no demuestra que sea la animación de aterrizaje de Dragon Adventure.

La entrada de mapa documentada en el manual de BT2 no incluye SELECT para cambiar zoom: el stick derecho rota la cámara y X aterriza en el punto de interés. Por ahora, los tres modos de distancia del prototipo no deben llamarse una reproducción confirmada del zoom del mapa de BT2.

## Diferencia de región y límite del análisis

BT2-Recomp identifica como objetivo la edición norteamericana SLUS-21441 y sus direcciones se basan en esa revisión. La ISO local es la edición europea SLES_541.64, con archivos `ZS2EU_*`. El lector de escenas de BT2-Recomp asume `ZS2US_1.AFS` y consulta la tabla de episodios en una dirección fija del ejecutable estadounidense; al leer el ELF europeo, esa tabla no aparece en esa dirección. Por tanto, sus direcciones y resultados dinámicos no se consideran automáticamente válidos para la ISO PAL.

El análisis de esta nota confirma los controles del manual y algunos enlaces de escena de la ISO PAL. Todavía no identifica las funciones exactas del ejecutable PAL que manejan la matriz/pivote de cámara, la aceleración de supervelocidad, el encuadre del personaje ni la animación de aterrizaje.

## Próximos pasos para cerrar las incógnitas

1. Rastrear la cámara del modo mapa en la edición exacta de la ISO SLES_541.64 o conseguir una revisión compatible con SLUS-21441 para utilizar BT2-Recomp directamente.
2. Registrar la posición/orientación del personaje y la matriz de cámara en reposo, al rotar cámara, al avanzar y durante L2.
3. Medir si la supervelocidad cambia solo la velocidad del jugador o también la distancia/posición de cámara.
4. Activar un destino y pulsar X, separando el momento de detección, la animación de aterrizaje, el fundido y la carga de escena.
5. Usar esos datos para diseñar un estado de exploración separado del bucle de batalla de BT3.
