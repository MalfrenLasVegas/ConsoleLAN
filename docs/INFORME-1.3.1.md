# ConsoleLAN 1.3.1 — rendimiento del acceso local

Validado el 6 de noviembre de 2026 en Windows 10 x64. Cambio limitado al camino entre el PC y sus consolas locales. La interfaz solo cambia su número de versión.

## Cambio

El auxiliar ya no procesa 128 paquetes y duerme incondicionalmente 2 ms al terminar cada vuelta. Esa espera puede alargarse con el temporizador de Windows y establecer un límite artificial de caudal.

Ahora vacía cada dirección en turnos acotados de 512 paquetes. Si agota el turno continúa inmediatamente con las colas, pasando antes por los controles y por las otras direcciones/adaptadores. Cuando las colas están vacías espera a los eventos reales `WintunGetReadWaitEvent` y `pcap_getevent`; el evento del padre tiene prioridad para el cierre. Las esperas mantienen acotada la revisión del canal y los latidos. No se modifica la resolución global de temporizadores ni la prioridad del proceso.

La captura del auxiliar usa un filtro propio: ARP, IPv4 dirigido al MAC reservado del acceso local y emisores extranjeros con la IP reservada del PC. Sus propias inyecciones masivas ya no vuelven a procesarse. La captura de la sala excluye en el kernel los paquetes ARP/IPv4 con el MAC reservado del acceso local, que el motor ya descartaba. Evita llenar su cola con una transferencia rápida. Las balizas de detección de otro puente se mantienen sin restricciones de MAC. No cambia el tráfico que admite el motor de la sala. Se conservan la detección de conflictos de `.1`, las conexiones iniciadas por las consolas, DNS y la salida opcional a Internet.

Se mantienen las políticas IP/MAC/Ethernet, MTU, puertos TCP permitidos, rutas `/32`, protección WFP, limpieza, pool de IP y protocolo de la sala. No hay cambios de Ethernet/VPN, NAT de usuario, firewall, interfaz gráfica ni idioma/tema aparte de nuevas traducciones de errores y versión. La excepción de PKG Sender autorizada durante el diagnóstico del usuario es independiente de esta release y no se instala ni amplía desde ella.

## Comparación de transferencia

`access_perf_tests` establece una conexión TCP real de Windows por un adaptador Wintun desechable. Una consola Ethernet simulada negocia ventanas TCP y confirma datos; se ejercitan `accessToEthernet`, `accessFromEthernet` y el mismo drenado/espera que usa el auxiliar. Se transfieren **32 MiB en cada sentido** y se comparan los bytes recibidos con el contenido esperado. No se leen ni escriben archivos de una consola física.

El modo `--legacy` reproduce la planificación anterior (128 paquetes y pausa fija) dentro del mismo test. No es una medición completa del binario anterior ni del enlace Ethernet físico; aísla el efecto de la planificación. El beneficio del nuevo filtro Npcap tampoco forma parte del caudal de esta simulación.

| Dirección | Planificación anterior | Eventos | Relación |
| --- | ---: | ---: | ---: |
| PC → consola simulada | 8.84 MB/s | 53.18 MB/s | 6.0× |
| Consola simulada → PC | 12.01 MB/s | 145.89 MB/s | 12.1× |

MB/s usa millones de bytes por segundo. El valor sintético de bajada puede superar el máximo de una Ethernet de 1 Gbps porque **no atraviesa una tarjeta ni una consola física**. No prueba ni promete gigabit con PS4/PS5. Datos completos: [performance-1.3.1.json](performance-1.3.1.json).

## Regresión

- CTest: núcleo (54 comprobaciones), política de acceso (59), tres sockets Winsock/relay, Internet y 11 casos Qt incluyendo inicialización/limpieza superados. Los dos tests Wintun se omiten en CTest sin administrador y se ejecutan aparte con elevación.
- Npcap: compilación del BPF real y comprobación de exclusión de inyecciones propias, conservación de respuestas TCP, tráfico de Internet, ARP y conflictos de `.1`. Apertura de captura con el filtro de producción y evento de lectura válido en una Ethernet real, sin inyectar tráfico. El filtro de la sala excluye el tráfico local reservado y conserva IPv4 de consolas, ARP y todas las balizas, incluso con MAC reservado.
- Wintun elevado: TCP/FTP pasivo de subida y descarga binaria, rutas y filtro WFP, Internet sin recrear adaptador, limpieza por desactivación, pérdida del canal, latidos y caída del padre. IP de interfaces existentes idéntica antes/después.
- Carga: un productor UDP de Windows llena el camino Wintun hacia un destino Ethernet simulado; el auxiliar sigue atendiendo latidos y cambios de Internet. Ese tráfico se descarta en el destino sintético y no llega a Ethernet/VPN reales.
- Internet: DNS público, proxy DNS UDP/TCP de `.1` y petición HTTP real superados. Las dependencias y el módulo de NAT son los mismos de 1.3.0.

## Aceptación física pendiente

El usuario detuvo su envío y pidió comprobar personalmente la velocidad con la PS4 después de instalar. No se sustituyó ni reinició su aplicación en uso. Instale el paquete completo para actualizar también `ConsoleLAN.Access.exe`, active el acceso local y repita el mismo envío con la misma IP/puerto y configuración. Compare MB/s sostenidos tras el arranque, manteniendo el mismo programa y consola; el resultado puede depender de Npcap, disco e instalador de la consola.

No cambie la subred ni las opciones de la VPN para probar esta corrección. No hace falta activar Internet para FTP/PKG local. El firewall del servidor del PC debe permitir las conexiones de la consola; el cambio de rendimiento no abre puertos.

## Reproducir

Compile con `scripts/build.cmd` siguiendo el README. Desde una consola de desarrollador **elevada**, con las DLL Qt en PATH y `wintun.dll` junto a los tests:

```powershell
.\build-x64\access_native_tests.exe --report build\native.txt
.\build-x64\access_perf_tests.exe --legacy --report build\legacy.json
.\build-x64\access_perf_tests.exe --report build\events.json
```

No ejecute a la vez los tests nativos y de rendimiento: usan el mismo adaptador desechable. Los informes de diagnóstico de la sesión real y datos del usuario permanecen fuera de GitHub y del paquete de fuentes.

Referencias primarias: [API Wintun](https://www.wintun.net/), [referencia de Npcap](https://npcap.com/guide/wpcap/), [esperas Sleep de Windows](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep).
