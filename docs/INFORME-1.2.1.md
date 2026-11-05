# ConsoleLAN 1.2.1 — conflicto IP/MAC por retransmisión del PC

## Problema y causa

Con el reenvío IPv4 de Windows activo, Npcap puede observar un paquete de una consola y una retransmisión del mismo PC. Esta segunda trama conserva la IP origen de la consola pero tiene la MAC Ethernet del PC. ConsoleLAN 1.2.0 podía aprender esa MAC como identidad de consola o bloquear una identidad correcta por un falso conflicto.

## Corrección

El motor recibe ahora la MAC de la interfaz de captura. Antes de aprender identidades o detectar conflictos, descarta las tramas cuya MAC origen es la del propio adaptador del PC. Esas tramas no se transportan por la VPN. La plataforma obtiene la MAC de Windows al abrir/actualizar la captura; si no puede identificarla, no abre esa captura sin protección.

La MAC real de una consola se aprende como antes. Una segunda MAC que no sea la del PC sigue provocando el conflicto. El registro de conflictos reales incluye las MAC guardada y observada y la interfaz, y el texto visible usa el nombre actual de la acción de detección.

No se desactiva el reenvío de Windows ni se cambian IP, DHCP, DNS, rutas, reservas, MAC del hardware o configuración de la consola. El adaptador local opcional conserva su aislamiento. Los conflictos son estado de sesión; al reiniciar la versión corregida se elimina un falso conflicto previo sin borrar la consola ni reasignar su reserva.

## Regresión

Una prueba sintética reproduce la trama con IP de consola y MAC del PC. Falló en el código anterior, antes de aplicar la corrección. Con el parche pasan cuatro comprobaciones nuevas: no aprender la MAC del PC, aprender la MAC real, no crear un conflicto por retransmisión y mantener el conflicto por otra MAC ajena. La batería de políticas pasa de 22 a 26 comprobaciones. Se ejecutan también el núcleo, Winsock y las pruebas de interfaz. La prueba nativa Wintun requiere elevación y se informa por separado de las pruebas normales.

El diagnóstico del caso real se realizó con metadatos de cabeceras, sin guardar contenidos de las conexiones. Los datos privados de esa captura no se incluyen en el repositorio ni en la release.
