# ConsoleLAN 1.2.0 — acceso local opcional

## Arquitectura

La LAN de juego conserva su protocolo y reservas. El acceso desde el PC es una vía local independiente: Wintun recibe paquetes IPv4 de Windows; una política de destinos permite solo las consolas locales válidas, y Npcap los entrega a su Ethernet con una MAC propia. Las respuestas IPv4 y las consultas ARP a la dirección del PC se procesan en sentido inverso. El motor de la sala excluye este tráfico del transporte VPN, tanto en captura como en recepción de tramas remotas.

El auxiliar elevado `ConsoleLAN.Access.exe` crea y posee el adaptador, su IP `.1/32` y las rutas `/32` de las consolas. No configura interfaces existentes, DHCP, DNS, puerta de enlace, NAT, firewall ni puentes Windows. El servidor de la sala no asigna `.1` a consolas. Los servicios de consolas remotas no se publican.

El proceso principal sigue sin privilegios. El canal es una named pipe local con ACL del usuario, Administradores y SYSTEM; se verifican los PID del cliente y servidor. No hay escucha TCP/UDP de control. Los mensajes tienen tamaño y tiempos acotados. El auxiliar valida nuevamente destinos, propiedad, subred, Ethernet física y solapamientos. Las capturas sintéticas existen únicamente en una biblioteca de pruebas compilada aparte, nunca en el auxiliar distribuido.

## Validación

- Regresión: 54 comprobaciones del núcleo y prueba Winsock de tres sockets.
- Política: 22 comprobaciones de propiedad local, rechazo de destinos remotos/Internet/broadcast, validación de IP/MAC/interfaz, ARP y exclusión de la VPN.
- Controlador real (elevado): adaptador Wintun firmado, selección de ruta e IP origen, FTP USER/PASS/PASV/RETR/STOR/QUIT mediante sockets TCP reales de Windows y descarga y subida binarias por conexiones pasivas independientes contra una consola simulada.
- Ciclo de vida real del auxiliar, con Ethernet simulada exclusivamente en el binario de pruebas: desactivación, pérdida del canal, expiración de latidos y caída del padre sin destructores. Se comprueba la retirada del adaptador y las rutas y la conservación de las direcciones previas.
- UI: habilitación solo con una consola local válida, estado activo, IP del PC, cancelación de permisos, idiomas, temas, navegación y vista compacta.

Las pruebas nativas requieren administrador y aparecen omitidas en CTest normal; se ejecutan además por separado con elevación. No se contabilizan como aprobadas por el mero hecho de omitirse. Las capturas públicas contienen datos de ejemplo. No se ha certificado una partida LAN ni una transferencia contra una PS5 física.

## Límites

Requiere Windows 10 x64, Npcap y una sala activa con consola local detectada. El servidor FTP se ejecuta en la consola y su puerto lo decide dicho servidor. FTP pasivo evita necesitar una regla entrante para el cliente del PC. El rendimiento y la compatibilidad del servidor real deben verificarse con hardware. El sondeo ARP no puede garantizar detectar equipos que no contesten. El controlador Wintun compartido puede permanecer instalado aunque el adaptador y sus rutas se retiren; no se elimina un controlador utilizado por terceros.
