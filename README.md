# Llenadora automática con control Wi‑Fi y Bluetooth

El proyecto conserva el cableado y los pines del sketch que ya funcionaba. Añade un panel web alojado por el propio ESP32 y una app Android que controla el mismo proceso por Bluetooth.

## Archivos

- `Firmware/LlenadoraESP32/LlenadoraESP32.ino`: firmware del ESP32 con la automatización existente, Wi‑Fi, servidor web y Bluetooth clásico.
- `Firmware/LlenadoraESP32/WebUI.h`: página web que muestra sensores y actuadores y permite controlar el sistema desde PC o celular.
- `AndroidApp/`: proyecto Android Studio para controlarlo por Bluetooth desde un teléfono Android.

## Pines y conexiones

No cambies las conexiones que ya tienes:

| Elemento | Pines del ESP32 |
|---|---|
| Motor paso a paso/driver | IN1 GPIO18, IN2 GPIO19, IN3 GPIO21, IN4 GPIO22 |
| Sensor HW‑201, salida OUT | GPIO27 |
| Servo del embudo, señal | GPIO23 |

El driver del motor, alimentación y tierra deben seguir como están en tu prototipo. No conectes las bobinas del motor directamente a GPIO. El servo debe tener alimentación suficiente y compartir GND con el ESP32. No alimentes un servo de carga desde el pin 3V3.

## Cargar el firmware

1. Abre `Firmware/LlenadoraESP32/LlenadoraESP32.ino` en Arduino IDE. `WebUI.h` ya está en la misma carpeta del sketch.
2. Selecciona la placa ESP32 que ya usas y el puerto USB.
3. Instala la biblioteca **ESP32Servo** desde el Library Manager si aún no está instalada. `WiFi`, `WebServer` y `BluetoothSerial` vienen con el paquete de placas ESP32 de Espressif.
4. Compila y carga el sketch. Al arrancar, el motor queda detenido y el embudo se coloca cerrado.
5. En el Monitor Serie selecciona **115200 baud**. Debe aparecer el nombre de la red y la dirección del panel.

## Usar el panel web por Wi‑Fi

1. Desde la PC o el celular, conéctate a la red **Llenadora-ESP32**.
2. Contraseña: **cajitas2026**.
3. Abre **http://192.168.4.1** en el navegador. El ESP32 crea esta red local; normalmente no tendrá acceso a Internet mientras estés conectado.
4. El panel actualiza cada segundo el estado del proceso, sensor, banda, embudo, velocidad y conteo. También permite iniciar/parar, cambiar tiempo de llenado y velocidad, dirección y hacer movimientos manuales por grados.

## Instalar y conectar la app Android por Bluetooth

1. Abre la carpeta `AndroidApp` en Android Studio y espera a que termine **Gradle Sync**. Si lo solicita, instala Android SDK Platform 35.
2. En Android Studio usa **Build > Build APK(s)**. El APK se crea en `AndroidApp/app/build/outputs/apk/debug/app-debug.apk`.
3. Instala ese APK en un teléfono Android. Si Android pregunta, permite instalar apps desde Android Studio o desde el administrador de archivos.
4. En Ajustes > Bluetooth del teléfono, empareja con **LlenadoraESP32**. Si el teléfono pide PIN, prueba `1234`; si no funciona, revisa la notificación de emparejamiento del dispositivo.
5. Abre **Llenadora BT**, selecciona el dispositivo emparejado y pulsa **Conectar**. La pantalla muestra telemetría en vivo y ofrece controles equivalentes al panel web.

La app incluida usa Bluetooth clásico SPP (`BluetoothSerial`), igual que el ESP32 original. Está pensada para Android y una placa ESP32 compatible con Bluetooth clásico (por ejemplo ESP32-WROOM/DevKit V1). Algunas placas ESP32-C3/S3 no ofrecen SPP clásico; si tu placa o teléfono es distinto, habrá que adaptar el transporte Bluetooth.

## Controles disponibles

La web y la app aceptan las mismas acciones:

- `INICIAR`: iniciar el ciclo automático de banda y llenado.
- `PARAR`: detener el motor y cerrar el embudo.
- `TIEMPO 2`: establecer segundos de llenado (0.5 a 10).
- `VELOCIDAD 4.88`: establecer velocidad en RPM (1 a 15, usada en movimiento manual).
- `DERECHA` / `IZQUIERDA`: seleccionar el sentido con proceso detenido.
- `GRADOS 90`: mover manualmente por grados (1 a 3600) con proceso automático detenido.
- `ESTADO`: solicitar el estado actual.

El panel web sirve para la plataforma PC y también se puede abrir desde el navegador del teléfono. La app Android aporta el control Bluetooth independiente del Wi‑Fi, para cubrir ambos enlaces requeridos.

## Verificación durante la demostración

1. Con motor detenido, conecta al panel web y muestra las lecturas en vivo.
2. Inicia el modo automático y presenta cómo el sensor detecta una cajita, la banda se pausa, el servo abre/cierra el embudo y la banda vuelve a moverse.
3. Muestra el incremento del contador de cajas.
4. Abre la app Android y repite iniciar/parar o ajusta el tiempo por Bluetooth.
5. Para evidenciar ambos enlaces por separado, desconecta la PC de la red del ESP32 y controla el prototipo desde la app Bluetooth.

Prueba primero sin producto y con el mecanismo libre de obstrucciones. Ajusta `ANGULO_CERRADO`, `ANGULO_ABIERTO` y el tiempo de llenado al mecanismo físico, sin cambiar los pines.
