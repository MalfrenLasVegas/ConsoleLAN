# ConsoleLAN 1.3.2 — lotes para el acceso local

Validado el 8 de noviembre de 2026 en Windows 10 x64. El parche acelera el acceso entre el PC que ejecuta ConsoleLAN y sus consolas físicas locales. No añade una IP a Ethernet ni cambia el transporte de las salas o la VPN.

## Implementación

El auxiliar reúne por Ethernet hasta 64 tramas del adaptador Wintun y las envía mediante `pcap_sendqueue_transmit(sync=0)`. No espera a llenar un lote: transmite las tramas disponibles y atiende la recepción, Internet y controles. Los ACK de descargas también aprovechan esta salida. La entrada hacia Windows sigue usando Wintun.

Solo `ConsoleLAN.Access` llama a `Capture::injectLocalBatch`. Las inyecciones de la sala, respuestas ARP y salida opcional de Internet conservan `Capture::inject` individual. Se mantienen validación IP/MAC, filtros, rutas `/32`, protección WFP, aislamiento de VPN y limpieza. No se cambian firewall, reservas, interfaz o preferencias, salvo el número de versión y traducciones de los nuevos errores.

La cola es acotada y reutilizable por captura. Las exportaciones de Npcap son opcionales: si no están disponibles o no se puede reservar la cola, se utiliza el envío individual antes de intentar transmitir. Un envío nativo incompleto es un error explícito; no se reenvía el lote completo, evitando duplicaciones.

## Pruebas

- CTest: siete suites superadas (núcleo, política local, despacho de lotes, Winsock con sockets reales, Internet, filtros/colas Npcap e interfaz). Las dos suites que requieren administrador se ejecutaron después con elevación y también pasaron.
- Lotes: 11 comprobaciones de éxito, ausencia de API, orden/binario del fallback, interrupción ante error y ausencia de reenvío tras fallo nativo. Las comprobaciones Npcap usan su cola real, verifican contenido/longitudes/orden/capacidad y rechazo de lotes inválidos antes de tocar el adaptador.
- Wintun elevado: TCP/FTP pasivo con subida y bajada binarias, rutas, protección contra bucles y limpieza al desactivar, perder canal, expirar latido o morir el padre. Internet se activa/desactiva bajo carga sin recrear el adaptador. Direcciones previas conservadas.
- Comparación física previa: archivos temporales de 128 y 512 MiB en PS5 real, orden invertido entre ejecuciones. Envío individual: 18,6–18,7 MB/s de subida; lotes: 59–67 MB/s según configuración. Lectura de vuelta y SHA-256 correctos.
- Verificación de la función de producción compilada: un arnés independiente usa `AccessAdapter`, políticas de producción y `Capture::injectLocalBatch`, con adaptador desechable y ruta de menor prioridad que el acceso habitual. FTP real con archivo temporal de **512 MiB**: **64,14 MB/s PC → PS5 y 114,24 MB/s PS5 → PC**. SHA-256 correcto, archivo borrado, cero descartes reportados por la captura y direcciones previas conservadas. [Resultados estructurados](performance-1.3.2.json).

El programa instalado no se sustituyó durante la validación. Se retiraron el adaptador, sus rutas y los archivos temporales; el acceso instalado siguió funcionando. Los resultados físicos se obtuvieron con Npcap 1.86 y una Ethernet de 1 Gbps, no con un receptor Ethernet simulado.

## Actualización y límites

Cerrar ConsoleLAN e instalar el paquete completo 1.3.2 para actualizar también `ConsoleLAN.Access.exe`. Abrir la aplicación, entrar en la sala y activar «Acceso desde este PC»; comprobar con el mismo cliente de archivos usado habitualmente. No se necesita configurar una IP secundaria en Ethernet.

Las cifras son de FTP y archivos de prueba en ese equipo. No garantizan velocidad sostenida con un juego de 50 GB, otro almacenamiento o PS5Upload. Aún no se ha validado una partida remota simultánea con este parche; esa aceptación se realiza con hardware y compañeros según [ACEPTACION.md](ACEPTACION.md). No se promete gigabit útil de subida.

Referencias: [colas nativas Npcap](https://npcap.com/guide/npcap-tutorial.html), [opciones de interfaz Winsock usadas por el arnés](https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-ip-socket-options).
