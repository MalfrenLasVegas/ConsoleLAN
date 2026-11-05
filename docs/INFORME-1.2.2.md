# ConsoleLAN 1.2.2 — TCP general y protección contra reentrada

## Diagnóstico

El adaptador ya transportaba IPv4/TCP, sin lista de puertos FTP. En un equipo con reenvío de Windows activo se observó un paquete TCP inyectado en Ethernet que volvía a la ruta Wintun con el mismo identificador y secuencia y TTL decreciente hasta agotarse. No era una retransmisión TCP normal ni falta de soporte para el protocolo. Las cabeceras se analizaron sin guardar datos de las conexiones; no se publican direcciones privadas, MAC ni nombres de archivos del usuario.

## Implementación

El auxiliar crea una sesión WFP dinámica, una subcapa propia y un filtro de bloqueo en `FWPM_LAYER_IPFORWARD_V4`, con una sola condición: `FWPM_CONDITION_IP_FORWARD_INTERFACE` igual al LUID del adaptador Wintun que acaba de crear. Bloquea únicamente el tránsito que intenta salir por ese adaptador, no el tráfico originado por sockets locales. No filtra por puerto. No desactiva el reenvío de Ethernet/VPN ni modifica reglas persistentes del Firewall de Windows. Si no puede instalar el filtro, retira el adaptador y deja el acceso desactivado.

El filtro se conserva hasta retirar las rutas y el adaptador, y se cierra con la sesión WFP. Los objetos de la sesión dinámica se retiran también al terminar el proceso. El auxiliar confirma la protección en el canal local; la aplicación rechaza un auxiliar antiguo que anuncie acceso activo sin ella. La API para retirar el filtro y la validación temporal sobre un adaptador existente solo se compilan en la biblioteca y ejecutable de pruebas, nunca en el auxiliar distribuido.

## Pruebas

- Núcleo, Winsock, UI y políticas existentes, incluida la regresión del falso conflicto IP/MAC.
- Seis comprobaciones adicionales conservan íntegramente paquetes TCP hacia los puertos 1, 21, 1337, 2121, 21210 y 65535: sin lista de puertos ni cambios de contenido. Total de políticas: 32.
- Dos adaptadores Wintun desechables reproducen el tránsito de Windows. Sin el filtro, el paquete reaparece en Wintun; con el filtro, no entra. Solo se habilita reenvío en el adaptador de prueba, nunca en los existentes.
- La inspección del filtro verifica capa, acción y condición de interfaz. Se comprueba su retirada al cerrar.
- TCP real de Windows contra una consola simulada: FTP en 2121, canales de datos pasivos independientes, descarga y subida binarias. Se repiten limpieza normal, pérdida del canal, latidos y caída del padre, manteniendo las direcciones existentes.
- Validación física: PS5, servidor FTP en 2121, saludo, login y listado pasivo correctos a través de la IP del adaptador ConsoleLAN. No se crearon ni modificaron archivos de la consola. Otro puerto TCP de la PS5 también aceptó conexión, sin asumir que fuera FTP.
- La corrección se validó en el equipo real mediante una protección temporal limitada al adaptador ConsoleLAN existente; se retiró automáticamente sin cambiar IP o rutas. La versión publicada incorpora la misma protección durante toda la sesión de acceso local.

El usuario ha comunicado partidas LAN reales satisfactorias entre casas. Esto es evidencia de su configuración, no una certificación universal. La prueba FTP física cubre conexión/login/listado; la integridad de transferencias se verifica con el par simulado y cada servidor físico debe validarse con sus propios archivos.

## Referencias de API

- https://learn.microsoft.com/en-us/windows/win32/fwp/filtering-condition-identifiers- — LUID de la interfaz de salida para tránsito.
- https://learn.microsoft.com/en-us/windows/win32/fwp/filtering-conditions-available-at-each-filtering-layer — condiciones de IPFORWARD.
- https://learn.microsoft.com/en-us/windows/win32/fwp/object-management — retirada de objetos de sesiones dinámicas.
