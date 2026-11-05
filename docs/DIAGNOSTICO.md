# Diagnóstico

## No se encuentra el anfitrión

Compruebe la IPv4 VPN del anfitrión y la elegida en ambos PC, no la IP de Internet ni la Ethernet doméstica. Use conexión directa si la VPN no propaga broadcast. Transporte: UDP 32190; búsqueda: UDP 32189. El socket de datos se enlaza a la IPv4 VPN; la recepción verifica con `IP_PKTINFO` el índice de la interfaz de llegada. La búsqueda recibe broadcast en su puerto dedicado y rechaza otras interfaces antes de procesar el protocolo.

En el firewall se necesitan permisos de entrada y, en instalaciones con salida restringida, de salida. El instalador ofrece una tarea desmarcada que solicita la IPv4 VPN local y limita las reglas al programa, UDP 32189-32190 e interfaz VPN. Si cambia el nombre de la interfaz o reinstala en otra ruta, actualice esa configuración. No se desactiva el firewall.

La versión portátil incluye `scripts/firewall.ps1`. Puede ejecutarse voluntariamente desde PowerShell con permisos de administrador:

```powershell
.\scripts\firewall.ps1 -ProgramPath 'C:\ConsoleLAN\ConsoleLAN.exe' -VpnAddress '10.147.0.8' -RemoteNetwork 'Any'
```

Sustituya las direcciones y la ruta. `RemoteNetwork` acepta una IP o una red IPv4/CIDR para reducir el alcance. Con `-Remove` retira exclusivamente reglas pertenecientes a esa misma ruta del ejecutable. No se necesita abrir puertos del router. Las políticas corporativas pueden impedir reglas locales; el instalador debe informar si su ejecución falla.

## Reserva sin MAC

Compruebe que la consola ha guardado la IP y máscara concedidas y que usa Ethernet. Genere tráfico de origen, por ejemplo entrando en modo LAN o repitiendo la prueba de red. Ver una pregunta ARP sobre su IP desde otro equipo no demuestra su identidad. Elija manualmente la Ethernet y aplíquela al registro si el aprendizaje automático resulta ambiguo.

## Npcap no abre

Revise versión, instalación y permisos Npcap. ConsoleLAN exige enlace Ethernet `DLT_EN10MB`, captura promiscua y modo no bloqueante. La VPN no es la interfaz de captura. No sustituya `wpcap.dll` por copias descargadas ni las copie junto al ejecutable. Si el controlador falla, reinstale Npcap desde su origen oficial. Los errores de apertura e inyección se registran.

## Hay tráfico observado pero no partida

Separe las capas: UDP/heartbeats solo prueban comunicación PC↔maestro; una MAC aprendida solo prueba que se observó un origen; una inyección realizada solo prueba que Npcap aceptó la trama. Valide ARP bidireccional, unicast sostenido y recepción real por el juego. Revise las limitaciones de switch, la MTU y los requisitos del modo LAN del juego.

No hay DNS, Internet, PSN ni traducción NAT. IPv6 y EtherTypes ajenos al alcance no se tunelizan. Las tramas VLAN y jumbo tampoco se admiten. Lea el protocolo para las condiciones de fragmentación.

## Reensamblados incompletos / envío MTU

Reduzca el presupuesto de datagrama de 1100 a 900 o 512 e inicie la sala con el nuevo valor. La cabecera propia ocupa 48 bytes, a lo que se añaden UDP, IP y la encapsulación VPN. `IP_DONTFRAGMENT` impide confiar en fragmentación IP de la red de transporte. No hay retransmisión de juego. La pérdida de un fragmento descarta su trama al expirar el reensamblado.

## Datos

Use **Abrir carpeta de datos** para localizar `state.json`, `consolelan.log` y el registro anterior. QSettings conserva las preferencias visuales en el perfil del usuario. Cada PC necesita una identidad propia: no clone `state.json` entre máquinas. El maestro rechaza una identidad simultánea repetida.

`state.json` guarda reservas del maestro y consolas locales mediante reemplazo atómico. Si está dañado, se conserva y se bloquean cambios hasta que se restaure una copia válida. Para empezar desde cero, cierre la aplicación y renombre el archivo; con ello cambiará la identidad del PC y podrá necesitar reconfigurar IP/reservas.
