# Desk Companion – firmware propio

Firmware hecho desde cero para ESP32-C3 Super Mini + OLED 1.3" (SH1106) + sensor táctil TTP223.
Reemplaza por completo al firmware original.

![expresiones](expresiones.png)

## Conexiones (igual que tu imagen)

| Módulo | Pin | ESP32-C3 |
|---|---|---|
| OLED | SDA | GPIO 8 |
| OLED | SCL | GPIO 9 |
| OLED / Touch | VCC | 3.3V |
| OLED / Touch | GND | G |
| Touch | OUT | GPIO 7 |

Los pines se cambian en `DeskCompanion/config.h`. Si tu pantalla es una SSD1306 de 0.96" en vez de una SH1106, descomentá `USE_SSD1306`.

## Instalar sin instalar nada

**Primera vez (por USB, desde el navegador):** entrá a **https://jorgefinelli90.github.io/desk-companion/** con Chrome o Edge en la compu, conectá la placa y tocá *Instalar*.
Si no aparece el puerto: mantené **BOOT** apretado mientras enchufás la placa.
Otra opción: en [esptool-js](https://espressif.github.io/esptool-js/), cargar `factory.bin` en `0x0`.

**Actualizaciones (por WiFi):** bajá `ota.bin` de la misma página y subilo en **http://deskbuddy.local/ota** (usuario `admin`, clave `deskbuddy`, se cambian en `config.h`).
Se conservan la cara elegida, el WiFi y los ajustes.

**Cómo se compila:** con cada push a `main`, GitHub Actions compila con PlatformIO, arma `factory.bin` (bootloader + particiones + app) y `ota.bin`, y publica la página con los binarios. Para sacar una versión nueva, subí `FW_VERSION` en `DeskCompanion/config.h`, editá lo que quieras (se puede hacer desde la web de GitHub) y esperá unos 3 minutos.

<details><summary>Compilar en tu compu (opcional)</summary>

- Arduino IDE: placa *esp32 by Espressif*, librería **U8g2**, abrí `DeskCompanion/DeskCompanion.ino`, placa **ESP32C3 Dev Module** con *USB CDC On Boot: Enabled*.
- PlatformIO: `pio run -e esp32c3 -t upload`.
</details>

## Primer arranque (WiFi)

1. Sin WiFi guardado, la pantalla muestra **Configurar WiFi**.
2. Desde el celu, conectate a la red `DeskBuddy-XXXX` (clave `deskbuddy`).
3. Se abre el portal solo; si no, entrá a `http://192.168.4.1`.
4. En **Ajustes → Buscar redes**, elegí tu WiFi, poné la clave y tocá *Conectar y reiniciar*.
5. Desde ahí lo encontrás en **http://deskbuddy.local** o en la IP que muestra la pantalla.

> Aunque no tengas router, en modo AP también funciona todo: cualquiera que se conecte a `DeskBuddy-XXXX` puede mandarle mensajes.

## Uso

**Caras según la hora (modo automático, viene activado):** cada 10 minutos elige una cara de la franja horaria. Más alegre desde las 9, se va cansando y enojando hacia las 18, y duerme de noche. La tabla está en `DeskCompanion/schedule.h` y la zona horaria en `config.h` (`TZ_INFO`, viene Argentina). Para que funcione necesita WiFi, porque toma la hora de internet.

**Página web** (`http://deskbuddy.local`)
- **🕘 Automático por hora**: lo prende o lo apaga.
- Tocar una cara la deja **fija** y apaga el modo automático. Se guarda aunque lo desenchufes.
- Mandar mensaje: nombre + texto (hasta 280 caracteres), con atajos rápidos y la opción de cambiar la cara junto con el mensaje.
- Historial con ✓ entregado / ✓✓ visto.
- Ajustes: nombre, brillo, rotar la pantalla 180°, WiFi, actualizar firmware.

**Sensor táctil**
| Gesto | Acción |
|---|---|
| Tap | Pasa a la siguiente cara (o cierra el mensaje que se está mostrando) |
| Mantener 1.5 s | Muestra la hora, la red y la IP |

El nivel del sensor en reposo se detecta solo al arrancar, así que anda tanto con TTP223 activo alto como activo bajo. No lo toques mientras lo enchufás.

**Cuando llega un mensaje:** durante 3 s la pantalla destella en blanco y muestra el sobre con "Mensaje de X", a brillo máximo. Después aparece el texto. Si es largo, se desplaza solo hacia abajo hasta el final, hace una pausa y vuelve a empezar. Queda 15 s (o más si hace falta para leerlo entero) y vuelve a las caras. Tocando el sensor se cierra antes.

**Reloj (opcional):** en la web, sección *Extras*, se activa *Mostrar el reloj*. Aparece cada 30 s durante 5 s; los dos valores se pueden cambiar.

**Modo compañero (opcional):** cada tanto (por defecto unos 20 min, al azar) hace una cara y le dice algo a Kharlita en un globito: "¡Hola Kharlita!!", "¡Buen día, Kharlita!", "¿Tomaste agua, Kharlita?"... Las frases cambian según la hora y de noche no habla. El nombre se puede cambiar en la web, y las frases se editan en `DeskCompanion/companion.h`.

Se aceptan acentos, ñ, ¿ y ¡. Los emojis comunes se pasan a texto (❤️ → `<3`, 😊 → `:)`, 👍 → `(y)`).

**WiFi:** las caras aparecen apenas lo enchufás y el WiFi se conecta solo en segundo plano. Mientras busca, titila un iconito arriba al centro. Si en 1 minuto no encuentra el router, además abre la red `DeskBuddy-XXXX` para que puedas reconfigurarlo, y sigue intentando con el router. Para olvidar el WiFi: Ajustes → *Olvidar WiFi*.

## API (para scripts, atajos del celu, Home Assistant, etc.)

```bash
curl "http://deskbuddy.local/api/msg?from=Ana&text=Hola!"
curl "http://deskbuddy.local/api/msg?from=Bot&text=Deploy%20OK&expr=excited"
curl "http://deskbuddy.local/api/expr?id=sleepy"   # deja la cara fija
curl "http://deskbuddy.local/api/auto?on=1"        # vuelve al modo por hora
curl "http://deskbuddy.local/api/state"
```

IDs de expresiones: `neutral happy joy love sad cry angry surprised scared sleepy sleeping wink confused suspicious thinking dizzy excited starstruck bored cool shy mischief dead`

## Agregar o modificar expresiones

Cada cara es una fila de la tabla `EXPRESSIONS` en `face.h`: tipo de ojo, tamaño, párpados (enojo/tristeza/sonrisa), hacia dónde mira, boca, animación y efectos (lágrimas, zzz, corazones, rubor, etc.). Si agregás una fila, aparece sola en la web.

## Notas
- Los mensajes se guardan en RAM (los últimos 12) y se pierden si se reinicia.
- Las Super Mini suelen tener problemas de WiFi por la antena; por eso viene activado `WIFI_LOW_TX_POWER`, que en la práctica mejora la conexión.
- En la Super Mini, el LED azul está en GPIO 8 (SDA), así que es normal que titile.
