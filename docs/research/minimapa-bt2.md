# Relación entre el mapa y el minimapa

La imagen `assets/ui/earth-minimap.png` procede de la captura local `work/raditz-avatar/overhead.png`: en los 543 428 píxeles opacos de la imagen editada, el color coincide exactamente con la captura. El usuario eliminó el fondo del agua y pidió mantenerlo transparente.

La captura se hizo con `work/raditz-avatar/overhead.py`: cámara ortográfica en (0, 0, 5000), rotación (0, 0, 0), `ortho_scale=5200` y resolución 1200 × 1200. La exportación convierte los ejes del juego a Blender, de modo que en la vista superior X del juego crece hacia la derecha y Z hacia arriba.

## Conversión

Para coordenadas originales del mapa BT2:

```text
pixel_x = 600 + 1200 * X / 5200
pixel_y = 600 - 1200 * Z / 5200
```

Para coordenadas del mapa de Adventure en BT3R, dividir primero X y Z por `earthScale` (actualmente 2). Para una interfaz que dibuja la imagen en otro tamaño, dividir los píxeles calculados por 1200 y aplicar el tamaño y posición de la imagen dibujada. La altura Y no interviene.

| Punto | X, Z originales | Píxel aproximado |
| --- | --- | --- |
| Aparición de Raditz | 1452, 650 | 935, 450 |
| Encuentro de Piccolo | 806, 278 | 786, 536 |
| Kame House | 1452, -1218 | 935, 881 |

La posición calculada de la cápsula coincide visualmente con el cráter y Kame House con su isla en la imagen. Es una validación de los puntos conocidos; queda comprobar el seguimiento dinámico durante el vuelo una vez integrada la interfaz. Piccolo y Raditz final comparten posición horizontal, aunque tengan alturas distintas.

La imagen está integrada en Adventure y conserva las áreas transparentes, sin fondo de agua. Una prueba con controles grabados mostró el minimapa y la flecha en movimiento. El mapa permanece fijo con el norte hacia arriba; la flecha usa 32 orientaciones. El zoom y los indicadores de misión aún no están implementados.
