# Protocolo ConsoleLAN v1

## Transporte e identidad

UDP IPv4, estrella: cada cliente habla exclusivamente con el anfitrión. Un único socket de transporte se enlaza a la dirección VPN seleccionada y usa `IP_UNICAST_IF` en orden de bytes de red. Se usa `IP_DONTFRAGMENT`, buffers limitados y E/S no bloqueante. Cada recepción con `WSARecvMsg` exige `IP_PKTINFO` y el índice de la VPN seleccionada.

El descubrimiento usa UDP 32189, puerto separado del transporte 32190. Para recibir broadcast, su socket escucha wildcard, pero solo entrega al protocolo paquetes `Discover` cuyo `IP_PKTINFO` corresponde a la VPN seleccionada. Todas las respuestas y búsquedas salen desde el socket de datos ligado a esa VPN. No se permite iniciar si no puede fijarse la interfaz de salida y validar la de entrada. No se realiza descubrimiento universal entre maestros ni se supone broadcast disponible en toda VPN.

La identidad persistente del PC y la de cada consola son UUID. El nodo y los tokens de sesión son aleatorios de 64 bits. El maestro conserva su ID de sala al reiniciar una subred conocida; las sesiones cambian, por lo que los datagramas antiguos no son miembros válidos.

## Cabecera (48 bytes)

Todos los enteros multibyte usan big endian. No se serializan estructuras C++ ni se depende de su alineación.

| Offset | Bytes | Campo |
| --- | --- | --- |
| 0 | 4 | Magic ASCII `CLAN` |
| 4 | 1 | Versión = 1 |
| 5 | 1 | Tipo |
| 6 | 2 | Tamaño cabecera = 48 |
| 8 | 8 | Sala |
| 16 | 8 | Token de sesión del enlace |
| 24 | 8 | Nodo de origen |
| 32 | 8 | Identificador/secuencia |
| 40 | 2 | Longitud total reensamblada |
| 42 | 1 | Índice de fragmento (base cero) |
| 43 | 1 | Número de fragmentos |
| 44 | 2 | Longitud de carga de este datagrama |
| 46 | 2 | Reservado = 0 |

Tipos: Discover=1, Offer=2, Hello=3, Challenge=4, Confirm=5, Welcome=6, State=7, Desired=8, Ping=9, Pong=10, Leave=11, Error=12 y Frame=13. `Frame` contiene Ethernet sin FCS. Los controles son objetos JSON UTF-8 encapsulados en el mismo protocolo binario versionado; no se interpretan como comandos del sistema.

## Handshake y controles

Hello contiene identidad, nombre y nonce del cliente. El maestro registra endpoint IP:puerto y devuelve Challenge con ese nonce y un desafío aleatorio. Confirm debe venir del mismo endpoint, nodo, sala y valores y llegar antes de 5 s. Solo entonces se añade al miembro y se entrega un token aleatorio mediante Welcome. El maestro rechaza identidades y nodos ya ocupados por otro endpoint. Los reintentos del mismo miembro pueden obtener Welcome otra vez.

Los clientes repiten Hello cada segundo hasta 9 s. Los controles de configuración son idempotentes: `Desired` comunica la lista completa de consolas deseadas cada segundo. El maestro reserva IP de manera autoritativa y publica `State` completo con revisión creciente. Los estados más antiguos no sustituyen a los nuevos. La configuración se recupera del siguiente ciclo si se pierde un control; no se retransmite tráfico de juego.

Los `Frame` de cliente deben coincidir con endpoint, sala, nodo y token de ese miembro. El cliente acepta datos únicamente del endpoint maestro y token negociado, y exige que el origen figure en los miembros conocidos. En ambos extremos se comprueba además MAC/IP/nodo contra las reservas del maestro antes de reenviar o inyectar.

Un ID de sala no autentica a una persona. Los tokens no aportan confidencialidad ni firma y se envían sobre la VPN de confianza. Un participante que pueda observar/instrumentar esa VPN tiene capacidades fuera del modelo de aislamiento. No se presenta el handshake como autenticación criptográfica de usuarios.

## Ethernet y reparto

Solo se admiten Ethernet sin VLAN, ARP Ethernet/IPv4 e IPv4 con tamaño máximo de trama 1514 bytes. El origen debe tener una reserva activa y MAC autorizada y coincidir con su IPv4 de origen o sender-IP ARP. ARP también exige coherencia entre su sender-MAC y la MAC Ethernet. No se aprende por target-IP.

Unicast conocido: solo a la sede del destino. Si origen y destino son consolas locales conocidas, no se genera copia VPN; el switch físico conserva la comunicación local. ARP/broadcast/multicast se difunden a sedes activas con consolas. El unicast desconocido se limita a 200 tramas/s por nodo y sede receptora; al superarlo se descarta. No se inyecta en la sede de origen.

Se bloquean UDP origen/destino 67/68. Para IPv4 UDP fragmentado, el primer fragmento debe incluir una cabecera UDP no DHCP; los siguientes solo se admiten si ese primer fragmento se observó para el mismo nodo/IP origen/IP destino/IP-ID en los últimos 5 s. La tabla tiene como máximo 512 entradas. Un fragmento huérfano o recibido antes del primero se descarta, sin cola adicional ni retransmisión. Otros protocolos IPv4 se transportan sin alterar su carga. Esta fragmentación IPv4 de la consola es distinta de la fragmentación de aplicación del túnel.

IPv6 (incluido DHCPv6), VLAN, LLDP y los demás EtherTypes se quedan en la LAN física y no entran al túnel. No hay traducción de direcciones, DNS, acceso a Internet, proxy ARP doméstico, servidor DHCP ni puerta de enlace virtual.

Las tramas remotas inyectadas mantienen su origen remoto. No coinciden con los registros locales autorizados para captura y no vuelven al túnel. Una baliza local `0x88b5` con `ConsoleLAN1` e ID de nodo detecta otro puente ConsoleLAN; el trabajador bloquea captura/reinyección en esa interfaz mientras observe el otro nodo (6,5 s de retención). La baliza no atraviesa la VPN. Es una detección auxiliar, no aislamiento de switch.

## Fragmentación de aplicación y límites

Presupuesto de datagrama configurable de 512 a 1400 bytes; predeterminado 1100 incluyendo cabecera. No se asume MTU 1500. Las tramas de juego y controles grandes se dividen en bloques con identificador, total, índice y cantidad. No hay retransmisiones propias del juego.

El reensamblador rechaza versión, tipo, cabecera, longitudes, flags, índices, cantidad y fragmentos duplicados contradictorios. Acepta desorden y duplicados idénticos. Límite 65535 bytes por mensaje, 192 fragmentos y 128 mensajes incompletos: menos de 8 MiB de carga pendiente, más metadatos acotados. Se descarta un mensaje incompleto tras 1200 ms. Un máximo de 4096 completados se conserva 10 s para deduplicar. Los handshakes iniciales y anuncios deben caber en un datagrama; Welcome y State pueden fragmentarse. Los nodos desconocidos no pueden abrir reensamblados de datos autenticados por sesión.

Las secuencias de datos se numeran por enlace, nodo de origen y destinatario. El relay conserva el origen y vuelve a numerar para cada receptor. Así, el tráfico dirigido a otro cliente no se cuenta como pérdida. Los huecos de hasta 4096 secuencias se mantienen 2 s para tolerar desorden y después se suman como pérdida estimada. No se detecta toda pérdida final de flujo ni se mide entrega física a la consola.

Se admiten 32 nodos por sala, 16 consolas por PC, 64 consolas activas por sala y 128 reservas recordadas por maestro. Son límites de recursos de cada maestro, independientes de los cinco anuncios visibles. Hay como máximo 64 handshakes pendientes, 64 anuncios recientes, 2048 datagramas de salida, 256 inyecciones pendientes y 64 comandos GUI pendientes. El trabajador procesa lotes limitados y vacía la salida en cada iteración para no acumular latencia. Los descartes son visibles en diagnóstico.

Ping/Pong cada segundo mide RTT al maestro. Tras 6,5 s sin maestro el cliente limpia sesión, colas de datos e inyecciones y detiene el reenvío. El maestro expira clientes silenciosos y retira su asociación activa, conservando sus reservas persistentes. No se elige otro maestro automáticamente.
