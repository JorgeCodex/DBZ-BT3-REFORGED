# Posiciones de los encuentros de Raditz en BT2

## Evidencia y conversión

Se consultó la ISO estadounidense de BT2 `SLUS_214.41` y el paquete local del mapa de la Tierra. En los guiones de mapa, los bloques de marcador de las escenas 33, 36 y 39 enlazan respectivamente con las escenas de historia 34, 37 y 40:

| Recorrido de BT2 | Escena de mapa | Marcador | Posición BT2 (X, Y, Z) | Posición horizontal BT3R a 2x (X, Z) |
| --- | ---: | ---: | --- | --- |
| Piccolo hacia Raditz | 33 | 14 | (806, 0, 278) | (1612, 556) |
| Goku hacia Kame House | 36 | 9 | (1452, 0, -1218) | (2904, -2436) |
| Goku y Piccolo hacia Raditz | 39 | 14 | (806, 0, 278) | (1612, 556) |

El script `tools/ps2recomp/story_scenes.py` de BT2-Recomp permite leer esas relaciones. La tabla de posiciones está en el bloque `3map` del paquete de Tierra de BT2: la rutina USA en `0x276B00` calcula `base + 0xD0 + índice * 0x20` y toma X y Z de los desplazamientos 0 y 8. La segunda posición del registro se usa en el cálculo de orientación. Los valores verticales de los registros son cero; en BT3R se calcula la altura visual mediante la colisión del mapa y se eleva el icono 45 unidades. Las coordenadas se multiplican por la escala real del mapa de BT3R, actualmente 2x.

## Vista de prueba actual

Se muestran a la vez Piccolo, Goku y Raditz a petición del usuario. Los encuentros de Piccolo y Raditz usan el mismo punto horizontal del marcador 14. En BT3R el objetivo de Raditz está 180 unidades por encima del de Piccolo para poder seleccionarlos por separado; se comprueba también la distancia vertical. El icono de Goku señala el marcador 9, Kame House. Se ocultan cuando quedan detrás de la cámara.

El marcador de Piccolo permite iniciar su pelea contra Raditz y volver al mapa; la victoria queda guardada de forma independiente. Después de esa victoria, el marcador de Kame House permite iniciar el combate como Raditz contra Goku, utilizando el escenario nativo de Kame House y el traje que corresponde a Goku en la misión original de BT3. La victoria se guarda y se vuelve al mapa como Goku; si se pierde se puede volver a intentar. Esta vista aún no implementa el rastreador, las cinemáticas intermedias ni a Piccolo como acompañante. El marcador elevado de Raditz permite confirmar y entregar el control a la pelea final nativa de BT3. En la progresión definitiva, la aparición del indicador de Goku sigue dependiendo de usar el rastreador y completar el encuentro con Piccolo.
