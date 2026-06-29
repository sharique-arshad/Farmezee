                    ☀ SOLAR POWERED FIELD NODE
┌───────────────────────────────────────────────────────────────┐
│                       Solar Panel                            │
│                            │                                 │
│                     TP4056 Charger                           │
│                            │                                 │
│                    Li-ion Battery                            │
│                            │                                 │
│                  MT3608 Boost Converter                      │
│                            │                                 │
│                         ESP32 MCU                            │
│                            │                                 │
│ ┌───────────── Sensors ─────────────┐    ┌──── Outputs ────┐ │
│ │ • BME280 (Temp/Humidity/Pressure) │    │ IRF520 Driver   │ │
│ │ • Soil Moisture Sensor            │──▶ │ Water Pump      │ │
│ │ • Rain Sensor                     │    │ Status LED      │ │
│ │ • MQ135 Air Quality               │    └─────────────────┘ │
│ └───────────────────────────────────┘                        │
│                            │                                 │
│                     RYLR998 LoRa Module                      │
└────────────────────────────┬──────────────────────────────────┘
                             │
                     Long Range LoRa
                             │
                             ▼
┌───────────────────────────────────────────────────────────────┐
│                    HOME RECEIVER NODE                         │
│                                                               │
│                ESP32 + RYLR998 LoRa Module                    │
│                           │                                   │
│     ┌──────────── User Interface ──────────────┐              │
│     │ OLED Display                             │              │
│     │ Crop Selection Buttons                   │              │
│     │ Manual Irrigation Button                 │              │
│     └──────────────────────────────────────────┘              │
│                           │                                   │
│     ┌──────────── Alert System ────────────────┐              │
│     │ DFPlayer Mini                            │              │
│     │ Speaker                                 │              │
│     │ Buzzer                                  │              │
│     └──────────────────────────────────────────┘              │
└────────────────────────────┬──────────────────────────────────┘
                             │
                        Optional Wi-Fi
                             │
                             ▼
                    ☁ Blynk IoT Cloud
                             │
                             ▼
                  Mobile / Web Dashboard
