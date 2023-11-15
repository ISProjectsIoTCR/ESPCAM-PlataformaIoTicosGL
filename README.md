# ESPCAM_WS_MQTT_PLATAFORMAIoTCR

Proyecto ralizado para equipar a los estudiantes con conocimientos avanzados para implementar y programar un ESPCAM para integrarlo con MQTT y WS.

## Paso 1: Instalacion de los paquetes para el ESP

- Abrir Arduino IDE
- Ir a la parte de archivos 
- Preferencias 
- En la parte de URL se pega el siguiente link: https://dl.espressif.com/dl/package_esp32_index.json
- Le damos aceptar 

## Paso 2: Descarga de la placa
- Herramientas
- Placa 
- Gestor de tarjetas 
- Buscar: esp32 
- Descargar la que dice: ESP32(by espressif Systems)

## Paso 3: Eleccion de la placa
- Herramientas 
- Placa
- ESP32 Arduino 
- Elegir la placa que dice: ESP Wrover Module

## Paso 4: Partition scheme
- Herramientas
- Partition scheme: Huge APP

 ## Paso 5: Debes tener un servicio WebSocketServer corriendo en local o tu instancia en nube

## Contribución

Este proyecto ha sido posible gracias a la colaboración de Andrés Hernández Chaves e Ignacio Cordero Chinchilla durante la práctica supervisada del año 2023.

## Problemas o Sugerencias

- Antes de subir la programacion debes desconectar la placa y conectar los pines IO0 con GND, despues conectas la placa y subes la programacion.
A la hora de ejecutar el programa desconectas esas conexiones.

## Librerias: 

- Splitter
- WiFi
- HTTPClient
- ArduinoJson
- PubSubClient
- esp_camera
- ArduinoJson
- WebSocketsClient
________________________________________________
## Conexiones del ESPCAM al Convertidor 
________________________________________________
    VCCIO del convertidor con los 5v del ESPCAM
________________________________________________
    TXD del convertidor con el U0R del ESPCAM
________________________________________________
    R0D del convertidor con el U0T del ESPCAM
________________________________________________
    GND del convertidor con el GND del ESPCAM
________________________________________________
