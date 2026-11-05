# ConsoleLAN 1.1 — cambios y validación

5 de noviembre de 2026. Versión de aplicación/instalador: **1.1.0**. El protocolo de red sigue siendo v1, compatible con 1.0.

## Cambios

- Tema claro y oscuro seleccionables en la cabecera, con paleta para estados activos/deshabilitados, tablas, selección, campos, registros y diálogos. La barra de título usa el modo oscuro por ventana cuando Windows lo permite.
- Español e inglés seleccionables en ejecución mediante un catálogo UTF-8 integrado y `QTranslator`. Se actualizan controles estáticos, cabeceras, estados, mensajes de diagnóstico, visor/exportación de registros y manual.
- Las preferencias se guardan de inmediato en `appearance/language` y `appearance/theme` bajo el mismo usuario de QSettings. No se reemplazan widgets ni se reinicia el trabajador al cambiar idioma/tema. Nombres introducidos por el usuario, IP, MAC, reservas e interfaces conservan su identidad.
- Manual inglés y README bilingüe; instalador en inglés/español, con el mismo AppId para actualizar desde 1.0.
- Corrección de referencias JSON temporales al refrescar tablas, evitando un posible cierre durante una sesión.
- Empaquetado versionado desde CMake y sumas SHA256 limitadas a los tres paquetes de esta versión. Las imágenes conceptuales originales y las capturas de interfaz se identifican por separado.

## Comprobaciones

Los resultados concretos de ejecución se adjuntan a la Release como `VALIDATION-1.1.txt`.

- **Núcleo:** las 54 comprobaciones de ARP/IPv4, reservas, tres nodos, validación de endpoint/sesión, fragmentación, pérdida/desorden/duplicación, límites y persistencia de la versión anterior.
- **Winsock nativo:** tres sockets reales, relay entre dos clientes, unicast bidireccional fragmentado, heartbeats, enlace exclusivo y rechazo de replay desde otro endpoint.
- **Interfaz Qt:** cambio de español a inglés y vuelta con una sesión y consola presentadas, manteniendo nombres, dirección del anfitrión y selección; tema oscuro/claro y colores activos/deshabilitados; preferencias leídas al crear otra ventana; catálogo con marcadores coherentes y mensajes formateados sin traducir nombres del usuario; renderizado de las cuatro combinaciones idioma/tema.
- Las capturas documentales se generan con el trabajador desactivado en el test y una etiqueta de adaptador de ejemplo, sin revelar los identificadores VPN del equipo. No representan una partida ni una prueba de red.
- Empaquetado/instalación: el ejecutable integrado debe coincidir en SHA256 con la compilación probada; prueba de arranque de los paquetes sin Qt de desarrollo en el PATH, manuales y catálogo incluidos, instalación por usuario y cierre/desinstalación sin activar reglas de firewall.

La validación se realiza en Windows 10 Pro 22H2 x64, Qt 6.12.0, MSVC 19.50, CMake 4.2.3 y el Npcap instalado en el equipo. Las nuevas traducciones son datos de ConsoleLAN embebidos en el ejecutable; no se añade un módulo Qt de traducciones externo.

## Compatibilidad y límites

Las reservas y preferencias existentes se mantienen al actualizar. No se cambia el formato persistente ni los mensajes del protocolo para localizar la presentación. Los detalles de errores proporcionados por Windows/Npcap y los nombres de adaptadores conservan su idioma original. Los registros originales en disco no se reescriben.

Se mantienen los límites de Ethernet/ARP/IPv4, una subred privada /24, 32 nodos, 16 consolas por PC y 64 consolas activas por sala, sin Internet/DNS/gateway/NAT ni migración automática del maestro. La [aceptación con dos PC físicos y consolas](ACEPTACION.md), incluida una partida real, sigue pendiente. El [informe 1.0](INFORME.md) se conserva como registro histórico.
