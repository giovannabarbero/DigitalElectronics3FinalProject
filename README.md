# Sistema de Medición de Nivel Sonoro - Sonómetro
> **Asignatura:** Electrónica Digital III - Universidad Nacional de Córdoba
> **Integrantes:**
> * Thomas von Büren - 43136697
> * Giovanna Luz Barbero - 46509956
> **Profesor:** Ing Marcos Blasco

---

## 🚀 1. Descripción General del Proyecto
Se toman mediciones del sonido ambiente mediante un micrófono, se procesa la información mediante estándares industriales aplicados a decibelímetros sonoros regulados, y se devuelve un valor en decibelios para ser expuesto mediante actuadores, indicadores visuales, e interfaces de comunicación. Posee un modo de auto-calibración.

### 🎯 Alcances del Proyecto (¿Qué hace y qué NO hace el sistema?)
* **El sistema SÍ es capaz de:** 
    * Adquirir información sonora ambiente en tiempo real.
    * Realizar cálculos de ponderación de curvas reguladas IEC 61672-1 y ANSI S1.43 sobre los datos obtenidos.
    * Calcular el valor en decibelios del sonido.
    * Mostrar valor y curva seleccionada en displays.
    * Mostrar el nivel en un Vu-Meter analógico y una tira ARGB LED NeoPixel.
    * Cambiar tau de adquisición y curva de ponderación *on the go*.
    * Inicializar modo de calibración para chequear correcta interpretación de valores ingresados.
    * Enviar String de información y recibir comandos mediante UART.
* **El sistema NO incluye (Fuera de alcance):** 
    * Almacenamiento local.
    * Portabilidad energética.
    * Conectividad por WiFi/Bluetooth/Ethernet.
    * Rigurosidad homologada ni certificada.

### ⏩ Posibles Etapas Siguientes (Líneas Futuras)
* Migrar el circuito de protoboard a un circuito impreso (PCB) diseñado bajo normas de compatibilidad electromagnética (EMC).
* Implementar componente portable con batería para mejorar portabilidad.
* Emplear componentes especializados de mejor calidad y ad hoc al dispositivo

---

## 📐 2. Arquitectura del Sistema: Hardware y Software

### 🔌 Hardware & Interconexión
* **Diagrama de Bloques:** [Insertar imagen o link al diagrama de bloques del hardware]
* **Esquemático del Circuito:** *[Inserte aquí la captura de imagen/render del esquemático completo desarrollado en KiCad/Altium]*
  `![Esquemático Completo](hardware/esquematico.png)`
* **Descripción del Circuito y Consideraciones de Diseño:** 
    * Pines de displays...
    * Diodo en Neopixel...
    * Decouple en bocina...
    * Botones y Led...
    * Decouple en Servo...
    * Amplificador de Micrófono:
        * Etapa bias y decouple... 
        * Etapa amp...
        * Etapa filtro...

### 💻 Arquitectura de Software (Firmware)
* **Diagrama de Flujo o Máquina de Estados:** *[Inserte aquí la imagen del diagrama que explique el lazo principal o el comportamiento del sistema]*
  `![Diagrama de Flujo / Máquina de Estados](docs/diagrama_software.png)`

---

## ⚡ 3. Especificaciones Eléctricas, Alimentación y Entorno

### 🔌 Parámetros de Alimentación y Consumo (Común a ambas materias)
* **Tensión de operación del sistema:** 5V / 3.3V
* **Método de alimentación:** Fuente externa de 12V con regulador de voltaje lineal AMS1117-5V/3.3V / Alimentación por USB
* **Consumo estimado o medido:** * En modo activo (máxima carga, relés/motores encendidos): `~ 1500 mA`

### 📌 Cortex-M3 / ARM
* **IDE y SDK:** MCUXpresso IDE v25.6 con CMSISv2p00_LPC17xx
* **Microcontrolador Principal:** NXP LPC1769 Rev D.
* **Bibliotecas de Terceros y Versiones:** CMSIS-DSP; Drivers customizados por David Trujillo.
* **Periféricos Avanzados Utilizados:** NVIC, GPDMA, DAC, ADC, GPIO, TIMER, PWM, SPI-MOSI, UART.
* **Estrategia de Concurrencia:** Bare-metal con Máquina de Estados Cooperativa

---

## 🔄 4. Proceso de Integración y Desarrollo (Común)
Describan cronológicamente cómo fueron sumando y testeando las diferentes partes del proyecto (enfoque modular de ingeniería).

* **Etapa 1 (Validación inicial):** [Ej: Configuración del oscilador/reloj y parpadeo de LED de estado].
* **Etapa 2 (Adquisición/Comunicación):** [Ej: Implementación del ADC y envío de tramas crudas por UART].
* **Etapa 3 (Integración lógica):** [Ej: Procesamiento de datos, lógica de control o montado sobre el RTOS].
* **Etapa 4 (Sistema Completo):** [Ej: Acople de actuadores finales, calibración y pruebas de estrés].

---

## 📊 5. Ensayos, Pruebas y Resultados (Común)
Demuestren con datos empíricos que el sistema funciona correctamente. **Es obligatorio incluir registro visual**.

* **Pruebas Funcionales Realizadas:** Detallen los ensayos (Ej: "Se inyectó una señal controlada para medir la precisión del ADC...").
* **Evidencia Fotográfica y Gráficos:** * *Capturas de instrumental:* [Insertar capturas de Osciloscopio, Analizador Lógico o Terminal Serie]
  * *Foto del Prototipo Real:* [Insertar foto del hardware final cableado/armado en funcionamiento]

---
