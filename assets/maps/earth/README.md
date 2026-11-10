# Mapa de la Tierra

El prototipo usa tres archivos locales derivados del mapa Dragon Adventure de Budokai Tenkaichi 2:

- `source-bt2-earth.unk`: paquete extraído de BT2.
- `converted-bt3-earth.unk`: paquete convertido para la estructura de escenarios de BT3.
- `runtime-raditz-earth.unk`: versión que carga el prototipo de vuelo.
- `Tierra Dragon Adventure.blend` y `.glb`: formatos de inspección.
- `Tierra en BT3.png`: imagen de referencia.

Estos recursos se mantienen en esta carpeta durante el desarrollo, pero `.gitignore` los excluye del repositorio público porque contienen datos derivados del juego. Los scripts para convertir y visualizar el mapa sí están en `tools/map-conversion/`. Para regenerarlos se necesitan los datos de juego locales apropiados.
