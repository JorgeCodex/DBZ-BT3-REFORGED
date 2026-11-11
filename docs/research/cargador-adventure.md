# Cargador propio de Adventure

El recorrido de carga de Adventure se reproduce en `src/adventure/adventure_loader.cpp`; su interfaz está en `adventure_loader.h` y se invoca desde `flight.cpp`. La compilación incluye ambos archivos. El flujo de las peleas continúa usando el cargador original.

## Partes reproducidas

- Preparación de la cola y el trabajo inicial de carga, incluidos los pasos de preparación del guion de historia.
- Solicitud de bancos de sonido compartidos, del escenario y de voz del personaje jugable.
- Solicitud del modelo y los parámetros del personaje jugable, memoria de sus miembros y preparación de los datos del escenario. No se solicita el modelo, la voz ni los búferes del oponente.
- Finalización del trabajo y liberación de los recursos que Adventure reservó. La configuración de la misión se restaura antes de regresar o iniciar una pelea.

El código reproduce la secuencia del cargador descompilado de BT3, pero llama a servicios nativos para leer archivos, interpretar el guion, cargar modelos y sonido, administrar memoria y otros recursos. También utiliza estructuras y direcciones internas concretas de la versión 0.1.39.6. Por tanto, cambiar el ejecutable o esas funciones puede romper Adventure aunque su secuencia de carga sea propia. No es una implementación independiente del motor.

Si no se puede instalar el trabajo propio, la ruta de compatibilidad recurre al trabajo de carga nativo; en ese caso no se garantiza omitir los recursos del segundo bando. Si el trabajo propio se instala pero falla al cargar, se cancela Adventure y se intenta liberar sus recursos. Los registros se escriben en `adventure-flight.log`.

## Validación

La carga, vuelo y limpieza se probaron en un entorno aislado. Una prueba instrumentada confirmó la transición a la pelea de Piccolo, la victoria guardada y la recarga de Adventure. La prueba forzó la llegada y el resultado para comprobar el ciclo; no sustituye una partida manual completa ni una comprobación auditiva de todos los sonidos.
