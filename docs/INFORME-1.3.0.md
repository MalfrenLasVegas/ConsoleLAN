# ConsoleLAN 1.3.0 — Internet opcional para consolas locales

Fecha de validación: 6 de noviembre de 2026. Windows 10 x64, MSVC C++20, Qt 6.12.0 y Npcap; se conservan el transporte LAN y la protección del acceso TCP/FTP de 1.2.2.

## Comportamiento

La función nueva permanece desactivada por defecto. Con una sala activa y una consola local detectada, se activa primero **Acceso local** y después **Internet para tus consolas**. La `.1` reservada de la sala actúa como gateway y proxy DNS; la consola conserva su IP reservada y máscara `/24`. La app muestra/copia gateway y DNS solo para consolas propias y cuando el auxiliar confirma el módulo activo. La configuración de la consola es manual. Ejemplo de documentación: consola `192.168.50.2`, máscara `255.255.255.0`, puerta de enlace y DNS primario `192.168.50.1`.

Cada sede usa su propio PC y su salida normal de Windows, independientemente del anfitrión y del túnel de la sala. No hay cambios de IP, DHCP, DNS, puerta de enlace ni reenvío en Ethernet/VPN. No se instala ICS, WinNAT, puente Windows, ruta por defecto del PC o apertura de router. El acceso FTP conserva su camino Wintun/Npcap hacia cualquier puerto TCP de las consolas locales; Internet usa sockets propios del auxiliar y NAT de usuario con libslirp 4.9.7.

Cambiar el interruptor de Internet abre/cierra solo esa pila, sin recrear el adaptador ni sus rutas o filtro WFP. Desactivar acceso local, salir de la sala, cerrar, perder el proceso principal/canal/latidos también retira el servicio. Un error de carga o polling del módulo se muestra en su apartado y no obliga a cerrar la sala. No se guarda Internet como activado para sesiones posteriores.

## Límites del tráfico

- Solo origen IP/MAC/Ethernet de consolas locales registradas y no en conflicto. El destino Ethernet del tráfico IPv4 debe ser el MAC del gateway.
- TCP/UDP salientes e ICMP IPv4 según soporte de Windows. DNS del gateway se admite en TCP/UDP 53. No hay DHCP, IPv6, TFTP, emulación de aplicaciones, UPnP, servidores del host expuestos por el NAT ni puertos entrantes.
- Destinos privados, loopback, link-local, multicast, broadcast y redes especiales/documentación excluidos del camino de Internet. Los servicios FTP/locales mantienen su camino anterior. Se excluyen opciones IPv4 y fragmentos dirigidos al DNS del gateway para evitar eludir la política de destinos/puertos.
- Respuestas solo hacia IP/MAC de consolas propias, por su Ethernet registrada. El gateway comparte el MAC del acceso local mediante normalización de cabeceras Ethernet/ARP; no cambia IP ni checksums al hacerlo.
- Todas estas tramas llevan el MAC reservado del acceso local y quedan fuera del transporte VPN. El pool de consolas continúa `.2`–`.254`; DNS y gateway comparten `.1` y no se ocupa otra reserva.

## Verificación

- Núcleo y política de acceso: 54 comprobaciones existentes y 59 comprobaciones de acceso/política Internet, incluyendo IP/MAC/Ethernet, destinos privados/especiales, DNS, opciones IPv4, fragmentos y exclusión del túnel.
- Tres sockets Winsock reales: coordinación, reservas, fragmentación y relay entre nodos conservados.
- Gateway sin administrador: primera transacción UDP desde una pila recién abierta, eco UDP binario de 512 bytes y TCP bidireccional de 16 KiB con checksums verificados y servidores reales controlados en loopback. Las excepciones de loopback existen solo en la biblioteca compilada para tests, fuera del auxiliar distribuido.
- Salida pública de lectura: consultas DNS directas y mediante la `.1`, respuesta DNS válida y HTTP real a un endpoint público mediante TCP. También resolución DNS sobre una conexión TCP real a la `.1`.
- Interfaz Qt: 11 casos incluyendo inicialización/limpieza, idioma/tema, valores de gateway/DNS y copia, estado desactivado, cambios durante sesión y capturas de diseños compacto/amplio con datos sintéticos. Ninguna captura publicada contiene datos del usuario.
- Prueba nativa elevada superada: Wintun firmado, protección WFP y control positivo de reenvío sin protección en adaptadores desechables, TCP real de Windows, FTP pasivo con subida/descarga binaria, activación/desactivación de Internet sin recrear adaptador/filtro y limpieza con Internet habilitado al desactivar, romper el canal, perder latidos o terminar abruptamente el padre. Las direcciones de Ethernet/VPN existentes coinciden antes/después. CTest normal omite únicamente esta prueba sin permisos; se ejecutó aparte con administrador. El control positivo se reintenta durante la puesta en marcha de las interfaces de prueba, para no depender de un único paquete antes de que Windows termine de aplicar rutas/filtros.

Se corrigió la traducción de prioridades de sockets a `POLLRDBAND`, ya que Microsoft Winsock no soporta `POLLPRI`. La pila se pulsa antes de recibir el primer paquete para inicializar su reloj: esto evita que la primera consulta UDP nazca con una expiración anterior al reloj real. Ambas correcciones se verifican con intercambios de datos, no solo con un estado de “activo”.

## Alcance de la aceptación

Estas pruebas no certifican Internet en una PS5 física, PSN, un tipo de NAT, Remote Play o todos los juegos online. No se ha cambiado la configuración de una consola real durante esta actualización. El usuario había comunicado partidas LAN reales satisfactorias y la versión anterior había validado conexión/login/listado FTP físico; son evidencias previas separadas de la nueva salida a Internet. La aceptación física pendiente consiste en introducir gateway/DNS indicados por la app y probar Internet y LAN por separado.

El PC debe estar encendido, conectado y con sala/acceso local/Internet activos. Las políticas, VPN de ruta por defecto y DNS de su conexión a Internet pueden afectar el resultado. No se garantiza NAT tipo 2 ni apertura de puertos: es salida saliente mediante sockets. Dependencias, licencias y fuentes exactas se incluyen en el instalador y portátil; consulte [DEPENDENCIAS.md](DEPENDENCIAS.md).

Fuentes primarias: [API y fuente libslirp](https://qemu.googlesource.com/libslirp/), [paquete fijado MSYS2](https://packages.msys2.org/packages/mingw-w64-clang-x86_64-libslirp), [WSAPOLLFD de Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/ns-winsock2-wsapollfd). Las fuentes de la revisión exacta, no solo la rama actual, se entregan con los paquetes.
