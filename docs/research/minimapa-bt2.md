# Integración del minimapa de BT2 en Adventure

Estado: investigación inicial; todavía no está integrado en el juego.

## Comprobaciones

- La referencia visual de Dragon Adventure de BT2 muestra el mapa de la Tierra abajo a la izquierda, un indicador del personaje y marcadores de destinos. Una captura local está en `C:\Users\jorge\Documents\Codex\2026-11-09\qu\work\bt2-camera-capture\map-no-pause.png`; contiene marcadores de una partida y no es la textura limpia.
- El módulo actual de Adventure dibuja texto y el marcador de destino en `src/adventure/flight.cpp`, función `DrawMarker`; aún no tiene una rutina de imagen o de minimapa.
- La descompilación de BT3 describe `Sprite_DrawList` en `src/sys/sprite.c` y su formato en `include/sys/sprite.h`. Su dirección PS2 es `0x126880`; esa dirección no es directamente la dirección x64 del ejecutable usado por el mod. No debe llamarse sin identificar y comprobar la dirección de la versión 0.1.39.6.
- El mapa jugable usa actualmente escala 2x. Cualquier transformación del mapa debe tomar coordenadas del mundo, escala, orientación y encuadre reales; no hay correspondencia de píxel a coordenada verificada todavía.
- El juego carga `external/local-only/DBZBT3R/earth-adventure-map.unk` (25 secciones). La copia experimental de 26 secciones en assets no es la que se usa en el lanzador.

## Trabajo pendiente

1. Localizar y extraer de la copia local de BT2 la imagen limpia del minimapa; verificar si se compone de más de una imagen. No usar una captura con marcadores estáticos incorporados como si fuera el recurso original.
2. Identificar la implementación x64 y ABI de dibujo de imágenes o preparar un recurso compatible con el motor; comprobar límites de memoria y el orden de dibujo sin interferir con las transiciones o el combate.
3. Calibrar la transformación de coordenadas y orientación con varios puntos conocidos del escenario, y comprobar el movimiento del indicador en ambos ejes y al girar.
4. Dibujar el objetivo actual únicamente cuando corresponda al estado de la historia. En el prototipo el objetivo C es provisional; el indicador de Goku tiene la condición de rastreador y combate contra Piccolo documentada en el proyecto.
5. Probar en Adventure, al llegar a C y durante la transición a la batalla, además de distintas resoluciones y ventana completa.

Los recursos derivados del juego deben permanecer locales y estar excluidos del repositorio público.
