# Dependencias, distribución y reconstrucción

Revisado el 5 de noviembre de 2026 con documentación de los proyectos:

| Dependencia | Versión utilizada | Uso y origen |
| --- | --- | --- |
| C++ / MSVC | MSVC 19.50.35728, toolset 14.50.35717 | Visual Studio Build Tools 2026, compilador x64, C++20 |
| Windows SDK | 10.0.26100.0 | Winsock, IP Helper y API Windows; destino Windows 10 |
| Qt | 6.12.0, msvc2022_64 | Core, Gui y Widgets, enlace dinámico |
| CMake / Ninja | Ver `build-info.txt` del paquete/informe de ejecución | Configuración y compilación reproducibles |
| Npcap SDK | 1.17 | Cabeceras; descarga oficial fijada por SHA256 en fetch-deps.ps1 |
| Npcap runtime local | 1.86 / libpcap 1.10.6, 64-bit time_t | Probado localmente; controlador instalado por el usuario |
| Inno Setup | 6.7.4 | Generación del instalador x64; descarga oficial firmada |
| Visual C++ Runtime | Redistribuible oficial x64 actual | Prerrequisito del instalador, firmado por Microsoft |

Windows mínimo para esta distribución: Windows 10 x64 2004 (19041); se ha construido en Windows 10 Pro 22H2 (19045). La compatibilidad con otras ediciones/equipos debe validarse. No se utiliza Qt Network para transportar tráfico: es Winsock nativo.

## Qt

Se usan módulos Qt Base bajo la opción LGPLv3 y enlace dinámico. La aplicación permite sustituir las DLL compatibles; no hay verificación propia que impida modificarlas ni prohibición de ingeniería inversa para depurar esas modificaciones. El código fuente completo de ConsoleLAN se entrega bajo MIT.

El paquete incluye `licenses/qtbase/` con los textos de licencia de la distribución fuente y `licenses/sources/qtbase-everywhere-src-6.12.0.tar.xz` con las fuentes correspondientes de Qt Base, incluyendo atribuciones de terceros y herramientas de reconstrucción. Las bibliotecas no se modifican por ConsoleLAN. El archivo se obtiene de la distribución oficial y se verifica contra SHA256. El instalador y ZIP contienen las mismas fuentes y avisos. La carpeta de plugins se limita a los necesarios para Widgets en Windows, sin módulos opcionales de otros paquetes Qt.

`windeployqt` también copia Qt Network de Qt Base y `dxcompiler.dll` como dependencias de ejecución de esta compilación de Qt. ConsoleLAN no usa Qt Network como transporte. El aviso de DirectX Shader Compiler/LLVM se incluye en `licenses/DirectXShaderCompiler-LICENSE.txt`, procedente de [su repositorio oficial](https://github.com/microsoft/DirectXShaderCompiler/blob/main/LICENSE.TXT).

Para reconstruir Qt, descomprima ese archivo y siga su README y la documentación oficial de construcción Windows para el mismo ABI MSVC x64. Para reconstruir ConsoleLAN, ejecute los scripts con `Qt6_ROOT` apuntando a esa compilación compatible. El empaquetador fija 6.12.0 para que las fuentes entregadas correspondan a las DLL; para cambiarla debe actualizar fuente, hashes y avisos.

Fuentes: [despliegue Windows](https://doc.qt.io/qt-6/windows-deployment.html), [obligaciones LGPL de Qt](https://www.qt.io/development/open-source-lgpl-obligations), [fuente Qt Base 6.12.0](https://download.qt.io/archive/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz).

## Npcap

**Npcap runtime y su instalador no se redistribuyen con ConsoleLAN.** Cada usuario debe descargarlo e instalarlo desde el proveedor. La documentación oficial publicada indica que la edición gratuita admite hasta cinco sistemas, con excepciones específicas, y prohíbe la redistribución externa. Para integrar su instalador o superar lo permitido debe obtenerse la licencia aplicable, por ejemplo OEM. No se presupone una licencia OEM ni se promete instalación silenciosa de la edición gratuita.

El SDK 1.17 se descarga durante la preparación del entorno y se mantiene fuera de los paquetes de código/binarios de ConsoleLAN. Se compila contra sus cabeceras, pero no se copia `wpcap.dll`, `Packet.dll`, servicios ni drivers. ConsoleLAN detecta la ausencia de Npcap al ejecutar y muestra diagnóstico y enlace oficial de instalación.

Fuentes: [descarga y licencia](https://npcap.com/#download), [guía del desarrollador](https://npcap.com/guide/npcap-devguide.html), [licencia publicada](https://github.com/nmap/npcap/blob/master/LICENSE).

## Microsoft y firewall

Se incluye únicamente el instalador oficial `vc_redist.x64.exe` firmado por Microsoft como prerrequisito de instalación, sin copiar DLL de una máquina de desarrollo. Para generar el paquete se comprueba su firma. Este archivo es un prerrequisito actualizable y sus bytes pueden cambiar en la URL oficial; `SHA256SUMS.txt` fija los paquetes concretos entregados. Se respetan las condiciones del runtime distribuido.

Las reglas de firewall son una tarea opcional desmarcada y requieren consentimiento durante la instalación. Se limitan a la ruta del ejecutable, UDP local 32189/32190, la interfaz que posee la IPv4 VPN indicada y los participantes permitidos. No hay apertura de router ni desactivación de firewall. El desinstalador elimina únicamente reglas cuyo filtro de programa coincide con su ruta. El script se entrega también para la versión portátil y no se ejecuta por abrirla.

Fuentes: [opciones IP de Winsock](https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-ip-socket-options), [New-NetFirewallRule](https://learn.microsoft.com/en-us/powershell/module/netsecurity/new-netfirewallrule), [runtime Microsoft](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist), [Inno Setup](https://jrsoftware.org/isdl.php).

Los binarios de ConsoleLAN no están firmados con certificado de editor. El paquete no incluye claves, certificados privados ni credenciales. No se han modificado en este entorno las reglas de firewall del usuario para probar la aplicación.
