# Herramientas de conversion del mapa

Scripts usados para convertir el escenario de la Tierra y preparar una version visible en Blender. Las rutas y dependencias locales pueden requerir ajuste en otra computadora.

- `convert_earth.py`: transforma la estructura del escenario BT2 para BT3. La opción `--scale 2` aplica una escala uniforme de 2x (el doble) respecto del paquete original; también desactiva las partículas de clima del template, que seguían a la cámara.
- `export_blender.py`: exporta el escenario convertido a glTF.
- `check_blender.py` y `save_blender_view.py`: comprobaciones y preparacion de la vista en Blender.
- `test_stage.ps1`: script auxiliar usado durante las pruebas de carga del escenario.

Para regenerar el mapa de vuelo, ejecuta este comando desde la raíz del repositorio.

```powershell
python tools/map-conversion/convert_earth.py assets/maps/earth/source-bt2-earth.unk assets/maps/earth/converted-bt3-earth.unk assets/maps/earth/earth-adventure-map.unk --scale 2
```

Copia el resultado a `external/local-only/DBZBT3R/earth-adventure-map.unk` con el juego cerrado.

## Colisión de Adventure

`build_collision.py` genera `earth-collision.bin` a partir del grupo visible de terreno y objetos del mapa convertido. Adventure consulta esa malla para el suelo, el movimiento y la cámara; no utiliza las consultas de colisión del escenario de combate. El océano sin geometría usa el nivel Y=0 como suelo provisional. El archivo generado se ignora en Git por contener geometría derivada del juego.

Para regenerar la colisión, utiliza exactamente el mapa que carga el lanzador:

```powershell
python tools/map-conversion/build_collision.py external/local-only/DBZBT3R/earth-adventure-map.unk assets/maps/earth/earth-collision.bin
Copy-Item assets/maps/earth/earth-collision.bin external/local-only/DBZBT3R/earth-collision.bin
```

Regenera la colisión cada vez que cambie la geometría o la escala del mapa. Es una primera aproximación basada en geometría visual: vegetación, superficies transparentes y huecos necesitan revisión manual en el juego. Existe una versión experimental de 26 secciones en `assets/maps/earth/earth-adventure-map.unk`; no es la copia de 25 secciones que utiliza actualmente el lanzador.

## Marcador de Raditz

El archivo `raditz-marker.bin` debe estar junto al ejecutable del paquete local. Para generarlo a partir del icono del proyecto y del mapa convertido se necesita Pillow (`python -m pip install Pillow`):

```powershell
python tools/map-conversion/build_raditz_marker.py assets/ui/raditz-icon.png external/local-only/DBZBT3R/earth-adventure-map.unk external/local-only/DBZBT3R/raditz-marker.bin
```
