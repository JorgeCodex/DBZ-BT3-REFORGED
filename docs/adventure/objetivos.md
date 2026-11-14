# Objetivos generales del modo Adventure

Estos objetivos describen sistemas compartidos por las misiones y sagas. Los eventos, combates y cinemáticas particulares se documentan en el archivo de cada misión. Una casilla marcada indica una implementación existente; las comprobaciones adicionales quedan pendientes por separado.

## Mundo y movimiento

- [x] Usar el mapa navegable de la Tierra de BT2.
- [x] Crear una primera malla de colisión para el terreno y objetos.
- [x] Establecer límites de movimiento y altura.
- [x] Implementar movimiento, orientación y cámara de seguimiento.
- [x] Implementar vuelo normal, medio y rápido, animaciones y efectos iniciales.
- [ ] Agregar sombreado al mapa.

## Flujo, progreso y encuentros

- [x] Ejecutar Adventure con su ciclo y cargador propios, reutilizando el motor de BT3.
- [x] Desbloquear encuentros según el progreso y el personaje controlado.
- [x] Confirmar encuentros con Cuadrado, con vuelo de llegada y fundido.
- [x] Guardar victorias y permitir repetir encuentros desbloqueados sin borrarlas.
- [x] Verificar el regreso a Adventure tras victoria, derrota y abandono en todos los casos.
- [ ] Generalizar el guardado y la transición de finalización para futuras misiones.

## Rastreador, minimapa e indicadores

- [x] Mostrar el marco, el filtro verde y tres triángulos amarillos del rastreador.
- [x] Mostrar destinos de Raditz cuando el rastreador está activado.
- [x] Mostrar posición y orientación del jugador en el minimapa.
- [x] Añadir puntos rojos y columnas 3D para encuentros disponibles.
- [x] Añadir contorno y resplandor al minimapa.
- [x] Corregir la alineación de los triángulos con el destino al girar la cámara.
- [x] Comprobar que puntos, columnas y destinos coinciden desde diferentes posiciones.

## Personajes y selección

- [x] Implementar un selector que conserva posición y progreso, sin recargar cuando no cambia la selección.
- [x] Permitir un acompañante con animaciones y efectos de vuelo.
- [x] Comprobar la posición y altura de los modelos y evitar duplicados.
- [ ] Mejorar las posturas de exploración.

## Alcance futuro

- [ ] Extender el modo a las demás sagas previstas.
