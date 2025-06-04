# Futbolin

Este proyecto implementa un marcador para futbolín utilizando un microcontrolador con WiFi (por ejemplo, un ESP32). El sistema detecta los goles mediante sensores ultrasónicos y muestra el resultado en una matriz de LEDs.

## Características
- Detección de goles con dos sensores ultrasónicos (HC-SR04 o similares).
- Visualización de los goles de cada equipo en una matriz `NeoMatrix` de 32x8.
- Iluminación de celebración y señales sonoras opcionales.
- Servidor web integrado para consultar el marcador y modificar:
  - Distancia de detección.
  - Tiempo necesario para resetear el marcador mediante un pulsador.
  - Intensidad de la matriz de LEDs.
- Configuración guardada en EEPROM para mantener los valores tras reinicios.

## Requisitos de hardware
- Placa compatible con WiFi (ESP32 se ha probado correctamente).
- Matriz de LEDs controlada con la librería `Adafruit_NeoMatrix`.
- Dos sensores ultrasónicos.
- Pulsador de reset.
- Fuente de alimentación adecuada.

## Compilación
Este código puede compilarse con [arduino-cli](https://arduino.github.io/arduino-cli/). Sustituye `esp32:esp32:esp32` por la FQBN correspondiente a tu placa:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 futbolin_sensor_proximidad.ino
```

## Uso básico
1. Modifica `SSID` y `PASS` en el archivo `futbolin_sensor_proximidad.ino` con los datos de tu red WiFi.
2. Sube el sketch a la placa.
3. Una vez conectado, abre un navegador y visita la dirección IP del dispositivo para ver el marcador y acceder a la página de configuración.

Este README busca facilitar el mantenimiento del proyecto. Si encuentras algún problema o mejora, cualquier contribución documentada será bienvenida.
