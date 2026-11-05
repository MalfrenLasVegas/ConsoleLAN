# Informe de entrega — ConsoleLAN 1.0

Fecha: 5 de noviembre de 2026. Proyecto en `F:\ConsoleLAN`.

## Funcionalidad implementada

Aplicación C++20/Qt 6 Widgets con interfaz española y motor nativo Windows. Todos los botones de la interfaz están conectados a operaciones reales: selección y actualización de adaptadores, crear/buscar/entrar/salir, conexión directa/reconexión, consolas recordadas, configuración copiable, interfaz manual, reaprendizaje explícito, eliminación, consulta/liberación de reservas desconectadas, exportación de diagnóstico y apertura de manual/datos.

El anfitrión coordina una sala en estrella, entrega reservas autoritativas y retransmite entre clientes. Se aprenden MAC e interfaz por IPv4 de origen o ARP sender-IP, se filtran orígenes y DHCP, se distribuyen ARP/broadcast/multicast y se dirige unicast conocido exclusivamente a su sede. El unicast local conocido no se tuneliza; el desconocido se limita por origen. Se detectan conflictos sin sustituir identidades silenciosamente.

Se implementan protocolo binario v1, handshake con desafío, tokens de sesión y comprobación de endpoint, fragmentación por presupuesto UDP, reensamblado limitado, caducidad, deduplicación, secuencias por destinatario, heartbeats y desconexión limpia. El trabajador mantiene E/S fuera de la GUI, limita las colas y cierra recursos. Hay persistencia atómica de consolas y reservas, preferencias por usuario y bloqueo de segunda instancia para el mismo perfil.

La distribución incluye ejecutable con sus DLL/plugins Qt, instalador x64, ZIP portátil, ZIP de fuentes, manual, diagnóstico, protocolo, licencias y guía de aceptación. Npcap se carga del sistema y se instala separadamente. El instalador ofrece consentimiento opcional para reglas limitadas a programa, VPN y UDP 32189-32190. Las fuentes Qt Base correspondientes se incluyen junto con sus avisos.

## Comprobaciones ejecutadas

Entorno real: Windows 10 Pro 22H2 x64, versión 10.0.19045; MSVC 19.50.35728; Windows SDK 10.0.26100.0; Qt 6.12.0 msvc2022_64; CMake 4.2.3; Ninja 1.13.0.git.kitware.jobserver-pipe-1; Npcap 1.86 basado en libpcap 1.10.6 con 64-bit time_t.

| Comprobación | Evidencia y alcance |
| --- | --- |
| Compilación Windows Release x64 | CMake/Ninja/MSVC; ejecutable nativo y bibliotecas compilados |
| Núcleo | **54 comprobaciones**: protocolo, fragmentación, desorden, duplicados, conflicto de fragmentos, expiración, memoria limitada, 10 000 datagramas aleatorios malformados, secuencias, ARP, DHCP, EtherTypes, tres nodos, aprendizaje por IP, relay entre dos clientes, broadcast/multicast, flooding limitado, tráfico doméstico excluido, recaptura, spoof de origen, dos consolas locales, fragmentación IPv4 no DHCP/huérfanos, sesión/endpoint/sala inválidos, nodo duplicado, timeout, reservas activas/inactivas, solapamiento de cliente, reinicio/persistencia dañada, interfaz automática/ambigua y selección manual, y State de 50 000 bytes con presupuesto 512 |
| Winsock real | Tres sockets reales en loopback, handshake/reservas/heartbeat, exclusividad del puerto, rechazo de replay desde endpoint diferente y unicast de 1442 bytes fragmentado A→maestro→B y B→maestro→A durante dos segundos de carga, con entrega de todas las tramas generadas y cero inyecciones unicast en maestro |
| Npcap real | DLL del sistema detectada; apertura de **Ethernet / Realtek** en modo promiscuo y no bloqueante con filtro BPF y enlace Ethernet aceptados; sin generar tráfico de consola durante esta comprobación |
| GUI | Arranque/cierre con trabajador real y captura PNG de la ventana; revisión visual del diseño español |
| Paquetes | El registro de empaquetado y las sumas SHA256 se entregan en `dist`; las comprobaciones finales del paquete se registran en `dist/VALIDACION-PAQUETE.txt` |

Las pruebas Ethernet del núcleo usan interfaces simuladas a propósito. La prueba Winsock usa **tres sockets dentro de un proceso**, no tres PC ni tres procesos. El laboratorio previo mencionado en el encargo no estaba incluido en la carpeta y no se presenta como ejecutado de nuevo.

Las tramas de Ethernet de los tests tienen cargas construidas para verificar decisiones de motor y transporte. No representan una consola ni una partida real y no se introducen como simulación en el funcionamiento normal de la aplicación.

## Límites concretos

- No se ha certificado una partida ni la comunicación física entre dos consolas en sedes distintas. Hacen falta dos PC, hardware compatible y seguir `ACEPTACION.md`.
- Abrir Npcap no demuestra que el switch entregue al PC todos los unicast relevantes ni que una consola reciba una inyección. La premisa de captura/inyección debe verificarse con el equipo real; puede necesitar mirroring.
- Ethernet/ARP/IPv4 sin VLAN, hasta 1514 bytes. IPv6, VLAN, jumbo y otros EtherTypes no se transportan. Los fragmentos UDP IPv4 posteriores requieren un primer fragmento no DHCP observado; los huérfanos/desorden previo al primero se descartan.
- Subred privada /24 por sala; 32 nodos, 16 consolas por PC, 64 consolas activas y 128 reservas persistentes por maestro. La GUI muestra cinco anuncios, sin imponer un máximo global de maestros.
- Un PC puente por LAN. La baliza de duplicados es auxiliar y no constituye aislamiento físico. Las consolas de un mismo switch se ven directamente, incluso en salas diferentes.
- Sala abierta sobre VPN de confianza. Los tokens y endpoints no son autenticación de personas ni cifrado adicional. No hay cuentas, contraseña ni servidor central.
- No hay NAT, DNS, Internet, PSN, proxy de servicios domésticos ni migración automática de maestro. Algunos firmwares exigen campos gateway/DNS o una prueba online para continuar; su modo LAN debe validarse expresamente.
- Descubrimiento solo donde la VPN propague broadcast; conexión directa disponible para el resto. Los filtros de recepción impiden aceptar búsquedas de otras interfaces.
- La pérdida indicada es una observación parcial del protocolo, no pérdida física completa ni entrega al juego. El RTT mostrado corresponde al maestro.
- Instalador y aplicación sin firma de editor propia. La instalación en un segundo Windows limpio, el consentimiento de firewall, las políticas corporativas y la desinstalación elevada con reglas reales requieren aceptación en ese equipo. En este entorno no se abrieron reglas de firewall para probarlos.

No se requiere que el usuario configure manualmente un puente, NAT o direcciones del PC. La configuración manual de las consolas y de la VPN existente sigue siendo parte de la puesta en marcha acordada.
