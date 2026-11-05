# ConsoleLAN 1.1.1 — revisión de interfaz

Estado: preparación local para revisión visual; la Release pública sigue siendo 1.1.0.

## Cambios

- Barra lateral con Jugar en LAN, Diagnóstico y Reservas; preferencias y ayuda siempre accesibles.
- Flujo en tres pasos: conexión, sala y consolas, con indicación del siguiente paso según el estado.
- Crear y unirse usan formularios separados. La búsqueda muestra instrucciones, progreso o resultados.
- Opciones avanzadas plegadas; selección explícita de VPN y Ethernet con explicaciones breves.
- Estados vacíos con acciones útiles y tabla de consolas reducida a nombre, IP y estado.
- Primera consola local seleccionada automáticamente. Panel de configuración con copia de IP y menú de gestión contextual; las acciones de una consola remota permanecen deshabilitadas.
- Disposición en dos columnas para ventanas amplias y una columna con desplazamiento para ventanas pequeñas.
- Tema claro/oscuro y español/inglés conservados, con aplicación inmediata y persistencia.

## Verificación

Compilación Windows x64 Release con MSVC y Qt 6.12.0. CTest: núcleo (54 comprobaciones), integración Winsock con tres sockets reales e interfaz Qt. Las pruebas de UI cubren preferencias, cambio de idioma durante sesión, navegación, opciones plegadas, estados vacíos, gestión local/remota y ausencia de desplazamiento horizontal a 1000 × 760.

Se revisan capturas de la interfaz real en los cuatro pares de idioma/tema, sala activa, vista compacta, unión a sala, diagnóstico y reservas. Los datos de las capturas son ejemplos inyectados en pruebas sin iniciar el motor de red. No constituyen una partida ni una conexión entre consolas verificada.

El protocolo, las reglas de transporte y el almacenamiento de consolas/reservas conservan el formato de 1.1.0. La validación con consolas y juegos sigue pendiente conforme a [ACEPTACION.md](ACEPTACION.md).
