# Manual de ConsoleLAN 1.2.0

[English manual](MANUAL.en.md)

## Idioma y tema

En la parte inferior de la barra lateral puede elegir **Idioma: Español / English** y **Tema: Claro / Oscuro**. Se aplican al momento, sin reiniciar ni abandonar la sala. El idioma actualiza etiquetas, estados, diagnóstico, registros visibles y el manual. No se traducen los nombres que haya escrito, las direcciones IP ni los nombres de adaptadores proporcionados por Windows.

Ambas preferencias se guardan inmediatamente para su usuario y se restauran al abrir. La primera vez se usa español si Windows está en español e inglés en los demás casos, con tema claro inicial. Actualizar desde 1.0 conserva consolas, reservas y las preferencias existentes.

El tema oscuro incluye tablas, campos, selección, controles deshabilitados, registros y diálogos Qt; la barra de título también se adapta en Windows compatible. Los detalles de errores nativos mantienen el idioma del controlador o de Windows. El archivo de registro conserva los mensajes originales; el visor y los mensajes legibles exportados siguen el idioma seleccionado.

## Navegación

**Jugar en LAN** reúne tres pasos: conexión, sala y consolas. **Diagnóstico** contiene los registros y herramientas de soporte; **Reservas**, las direcciones gestionadas por el anfitrión. Las opciones de subred y presupuesto UDP están plegadas en **Opciones avanzadas**. Seleccione una consola para ver su IP y copiar la configuración; **Gestionar** agrupa las acciones de Ethernet, MAC y eliminación.

## Preparar cada sede

Conecte por cable el PC y las consolas al mismo switch. La VPN debe estar operativa en los PC y permitir UDP entre sus direcciones. Las redes domésticas pueden tener rangos diferentes o coincidir; no hay que modificarlas. Se admite un único PC ConsoleLAN por LAN física.

Instale Npcap en cada PC desde https://npcap.com/#download. La opción de compatibilidad WinPcap no es necesaria. Si Npcap se instaló restringido a administradores, su acceso puede requerir elevación. ConsoleLAN usa la DLL nativa de Npcap en `System32\Npcap`, no una DLL encontrada en el directorio actual.

La premisa de funcionamiento es que el hardware permita capturar e inyectar las tramas relevantes. El modo promiscuo no obliga a un switch ordinario a enviar al PC un unicast que haya aprendido entre otros puertos. Compruebe el equipamiento en la aceptación real; puede necesitar un switch con mirroring o una disposición compatible con esta premisa.

## Crear y entrar

1. Elija **VPN de los PC** por nombre e IPv4. No elija la Ethernet doméstica como VPN. Los adaptadores desconectados aparecen para diagnóstico, pero no sirven para iniciar.
2. En el anfitrión, elija **Crear una sala**, escriba el nombre y, en **Opciones avanzadas**, una subred privada /24, por ejemplo `10.253.42.0`. ConsoleLAN comprueba interfaces y rutas conocidas para evitar solapamientos. No modifica IP, rutas, router, NAT ni crea un puente Windows.
3. Pulse **Crear sala**. Los demás eligen su VPN, **Unirme a una sala** y **Buscar salas en mi VPN**. Se presentan hasta cinco resultados recientes, sin establecer un límite mundial de salas.
4. Haga doble clic en un resultado o escriba la IPv4 VPN del anfitrión y pulse **Conectar / reconectar**. Las VPN que no transportan broadcast, como muchas configuraciones WireGuard, requieren conexión directa. Puerto normal: 32190; se acepta `IP:puerto` para diagnóstico.
5. Revise nombre de sala, maestro y participantes. Un PC participa en una sala a la vez. El instalador puede autorizar el firewall exclusivamente para la aplicación y la interfaz VPN elegida, con consentimiento explícito.

La sala es abierta a participantes de la VPN. No hay autenticación de cuentas, contraseñas de sala ni cifrado adicional. La VPN aporta el aislamiento de confianza. No exponga estos puertos a Internet.

## Añadir y configurar consolas

Pulse **Añadir consola**, escriba un nombre y seleccione la fila. Puede recordar hasta 16 consolas por PC. La reserva la concede el maestro; no configure una dirección que siga apareciendo como pendiente.

**Copiar configuración IP** copia la dirección concedida, máscara `255.255.255.0` y las indicaciones de gateway/DNS. El rango asignable es `.2` a `.254`; `.1` se reserva, pero no ejecuta un router ni un DNS.

En la consola elija Ethernet, configuración manual y la IP/máscara mostradas. Deje gateway y DNS sin configurar o en `0.0.0.0` si su sistema lo admite. ConsoleLAN no proporciona DNS ni salida a Internet. No hay un servidor oculto en `.1` y no se debe copiar el DNS doméstico esperando que funcione a través del túnel.

Algunas consolas obligan a escribir valores de gateway/DNS antes de guardar. Cuando admitan una configuración LAN sin servicios, puede usar la IP propia de la consola como gateway y DNS **solo como marcador de configuración**, sabiendo que no hay servicio en esas direcciones y que la prueba de Internet fallará. Eso no aporta Internet, DNS ni garantía de que el firmware conserve la configuración. Si la consola no permite continuar hacia el modo LAN sin validar esos servicios, esa combinación de firmware/juego requiere una solución de red adicional fuera de ConsoleLAN 1.2.0; no se muestra como compatible sin probarla.

La prueba de Internet puede fallar con la LAN por cable disponible. Entre en el modo LAN/System Link del juego; no en sus modos PSN/online. Los requisitos de cada juego (versiones, contenidos, límites de latencia y modo LAN) se validan por separado.

## Aprendizaje y estados

ConsoleLAN aprende una MAC cuando ve IPv4 **de origen** con la IP reservada, o ARP cuyo **sender IP** sea esa IP y cuya MAC remitente coincida con la Ethernet. Una pregunta ARP sobre la dirección reservada no identifica a la consola que la posee.

Con Ethernet automática, el motor abre las interfaces físicas activas y recuerda aquella donde ve ese origen. Si el origen aparece en varias interfaces se marca conflicto. Elija la interfaz física correcta y pulse **Aplicar Ethernet elegida**. Seleccionar el desplegable por sí solo establece la preferencia para la próxima consola; este botón cambia el registro seleccionado.

Se recuerdan nombre, identidad, IP preferida, MAC e interfaz. Una MAC recordada no demuestra tráfico en esta sesión. **Tráfico observado** tampoco demuestra conectividad con otra consola. Confírmela con ARP bidireccional, unicast y la partida de aceptación.

Si otra MAC usa una reserva, el motor muestra conflicto y no sustituye la asociación. Revise la configuración de ambas consolas. **Reaprender MAC** crea una nueva identidad de reserva para esa consola y retira la anterior cuando se sincroniza con el maestro; puede cambiar su IP si otros registros la retienen. **Eliminar / olvidar** elimina la consola local y solicita liberar su reserva. Los registros remotos se gestionan en su PC propietario.

El anfitrión dispone de **Reservas del maestro** para consultar también sedes desconectadas. Puede liberar explícitamente una reserva desconectada tras revisar su IP/MAC. Las reservas de sedes activas solo se eliminan desde su PC propietario. Liberar una dirección permite reasignarla; si vuelve la consola antigua debe revisar su configuración.

## Cambiar sala o recuperar desconexiones

Pulse **Salir**, cambie VPN o sala y conecte otra vez. Las consolas recordadas solicitan mantener sus IP. Si el maestro nuevo no puede conservar una dirección, se muestra la nueva y se registra **Reconfigurar: IP anterior → IP nueva**. Compruebe siempre la fila al cambiar de sala.

Cuando el maestro cierra o deja de responder durante unos 6,5 segundos, el cliente detiene el reenvío. No hay migración automática. Restaure la VPN/maestro y pulse **Conectar / reconectar**. Un cambio de IPv4 VPN exige volver a seleccionar el adaptador actualizado.

## Contadores y diagnóstico

TX cuenta tramas locales admitidas para el túnel; RX, tramas remotas admitidas por el motor. No son bytes de la VPN ni pruebas de entrega a la consola. Las inyecciones y errores Npcap aparecen por separado en Diagnóstico. La latencia es el RTT de control **al maestro**, nunca el ping entre consolas.

**Incompletas** cuenta reensamblados expirados; **saltos estimados** detecta huecos en las secuencias por enlace tras una ventana de desorden. Un datagrama completamente perdido al final de un flujo puede no generar hueco detectable. No es una medición completa de pérdida del juego.

El presupuesto UDP predeterminado es 1100 bytes, incluida la cabecera propia. Puede reducirlo a 512 para VPN con MTU pequeña. Cambie el valor antes de iniciar o buscar; el siguiente comando enviado al motor lo aplica. El tráfico de juego no se retransmite.

En **Diagnóstico** puede abrir los datos, consultar Npcap y capturas, y exportar un informe con el estado y el registro. Contiene IP, nombres, MAC e identificadores; no se graba el contenido de las tramas. Los registros rotan alrededor de 2 MiB y conservan el anterior.

## Aislamiento

Los mensajes de sala y sesión y los endpoints se verifican en el túnel. Un unicast local conocido se queda en el switch; uno remoto conocido solo se entrega a su sede. Broadcast, multicast y ARP se distribuyen a las sedes de la sala con consolas. El unicast desconocido está limitado a 200 tramas/s por origen.

El filtro del túnel no aísla puertos del switch: dos consolas de la misma LAN física pueden verse directamente aunque sus PC hayan creado salas diferentes. Las balizas locales detectan otros puentes ConsoleLAN y bloquean la interfaz afectada mientras se sigan viendo, pero no sustituyen el requisito de un único PC por LAN.

## Acceso del PC a sus consolas (1.2)

Este acceso es opcional, está desactivado al arrancar y funciona mientras la sala está activa. Primero añada la consola y espere a que tenga IP, MAC y Ethernet detectadas. Después pulse **Activar acceso local** en **Acceso desde este PC** y acepte el aviso de administrador de Windows para `ConsoleLAN.Access.exe`.

La aplicación crea **ConsoleLAN Local**, un adaptador Wintun con la dirección `.1` de la subred de la sala y rutas `/32` únicamente a las consolas locales detectadas. `.1` ya estaba excluida de las reservas de consola: no es un router ni un DNS. No cambia las direcciones, DHCP, DNS o puerta de enlace de la Ethernet ni de la VPN. Antes de activarse comprueba solapamientos y busca conflictos de `.1` mediante ARP.

Cuando aparezca **Acceso local activo**, abra su cliente FTP y use la IP reservada de la consola y el puerto de su servidor FTP. El servidor FTP debe estar activo en la consola. Se recomienda modo pasivo; el acceso no abre puertos de firewall para servidores del PC. El tráfico de juego puede continuar por ConsoleLAN. Esta función no permite acceder a consolas de otras casas.

**Desactivar acceso local**, salir de la sala o cerrar ConsoleLAN retira las rutas y el adaptador propios. El proceso auxiliar también se detiene si pierde a su proceso principal, el canal de control o sus latidos. Espere a que desaparezca el adaptador antes de cambiar de subred o crear otra sala. No se desinstala el controlador Wintun compartido: otras aplicaciones pueden utilizarlo.

Si Windows cancela o deniega la elevación, el acceso queda desactivado y la sala continúa funcionando. Si la consola pierde la IP/MAC/Ethernet válida, el acceso se detiene. Tras corregir el problema, puede activarlo de nuevo. El acceso solo es IPv4; no instala puentes Windows, NAT ni una ruta por defecto.

La prueba automatizada de 1.2 utiliza el controlador firmado y TCP de Windows para una transferencia FTP pasiva contra una consola simulada. No sustituye la prueba con el servidor FTP de su PS5 ni certifica una partida entre consolas.
