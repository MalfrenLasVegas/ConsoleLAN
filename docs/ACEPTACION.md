# Aceptación real con dos PC y consolas

No marque los casos siguientes como aprobados por haber pasado CTest. Requieren hardware y una VPN entre dos sedes. Registre fecha, versiones de Windows/Npcap/VPN, modelo de switch, consola, firmware, juego y versión. Conserve diagnóstico de ambas sedes y, cuando proceda, capturas Wireshark obtenidas con consentimiento.

| Caso | Procedimiento | Evidencia exigida |
| --- | --- | --- |
| Interfaces | Seleccionar VPN y Ethernet diferentes; crear/entrar por IP VPN | Socket en IPv4 VPN, capturas solo en Ethernet elegidas; ningún UDP ConsoleLAN en otra interfaz |
| Descubrimiento | Buscar en VPN con broadcast habilitado; repetir bloqueándolo | Sala visible si hay broadcast; conexión directa operativa si no lo hay |
| Reservas | Añadir una consola en cada sede; reiniciar y reconectar | IP únicas, IP preferidas conservadas o aviso claro de reconfiguración |
| MAC | Configurar manualmente las IP; generar tráfico Ethernet | MAC aprendida de sender-IP/origen correcto en cada sede; una pregunta ARP desde otra IP no aprende la MAC |
| ARP A→B | Vaciar cachés/reiniciar red de consola A y provocar conexión hacia B | Petición en B, respuesta en A y asociación IP/MAC correcta |
| ARP B→A | Repetir desde B | Respuesta inversa y resolución consistente |
| Unicast | Generar tráfico sostenido en ambas direcciones durante 10 minutos | Captura/inyección sostenida, sin crecimiento de colas; destino exclusivo en la otra sede; datos recibidos por la consola/juego |
| Broadcast/multicast | Abrir búsqueda de partidas y un modo que lo utilice | Descubrimiento del juego en ambos sentidos; no aparecen dispositivos domésticos ajenos |
| MTU | Probar presupuesto 1100 y 512; enviar tramas cercanas a 1514 bytes | Datagramas dentro del presupuesto; reensamblado correcto o pérdidas diagnosticadas, sin asumir MTU 1500 de VPN |
| DHCP | Tener DHCP doméstico activo, lanzar detección/configuración de consola | Ninguna oferta DHCP doméstica atraviesa el túnel; las reservas siguen manuales |
| Dos consolas locales | Añadir A1 y A2 en el switch de A y B1 en B | IP/MAC distintas; A1↔A2 por switch sin unicast en túnel; ambas alcanzan B1 |
| Tres nodos | Añadir una tercera sede C como cliente | B↔C pasa por A maestro, sin conexión directa B↔C |
| Conflictos | Configurar temporalmente otra consola con la misma IP/MAC; seleccionar otra interfaz | Error visible y ausencia de sustitución silenciosa; recuperación explícita |
| Maestro perdido | Cerrar/finalizar el maestro o cortar su VPN | Reenvío detenido y aviso tras timeout; reconexión manual funcional |
| Cliente perdido | Cortar la VPN de un cliente | Expiración en maestro; reservas recordadas y recuperación al volver |
| Sala distinta | Crear otra sala en una LAN físicamente distinta | No atraviesan tramas de la otra sala; no se interpreta como aislamiento de consolas en un mismo switch |
| Puente duplicado | Abrir ConsoleLAN en dos PC del mismo switch | Baliza y aviso de duplicado; interfaz bloqueada; al retirar uno se recupera tras expiración |
| Juego | Abrir modo LAN/System Link, crear partida e incorporarse desde la otra sede | Partida real de al menos 15 minutos, con dos consolas locales si el juego lo permite |
| Instalación | Instalar en un segundo Windows limpio con Npcap; consentir reglas para la VPN | Ejecutable abre sin Qt de desarrollo; reglas limitadas; quitar aplicación retira sus reglas y conserva los datos |

Si falla unicast mientras broadcast/ARP llegan, compruebe primero si el switch entrega realmente esas tramas al PC puente. Modo promiscuo por sí solo no garantiza visibilidad de todos los puertos. Si falla el modo LAN por validación obligatoria de gateway/DNS o por requisitos del juego, documente esa limitación específica.

Resultado por juego: `NO PROBADO`, `APROBADO` o `FALLIDO`, junto con evidencias. El resultado de un juego no certifica los demás.
