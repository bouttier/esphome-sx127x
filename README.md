# ESPHome component for Semtech LoRa SX1276/77/78/79 transceivers

This component is based on the nice [sx127x](https://github.com/dernasherbrezon/sx127x) library from @dernasherbrezon, which let you use your own SPI implementation, allowing a seamless integration with ESPHome.

Successfully tested on `DFRobot FireBeetle 2 ESP32-C6` dev board with an `Ra-02` module and on `LILIGO LoRa32` board.

## Features

- Based on an external [sx127x](https://github.com/dernasherbrezon/sx127x) library.
- Send and receive LoRa packet only for now but FSK/OOK can be added later as supported by underlaid library.
- Async send action.
- Automatically going back to previous opmod after a transmission.
- Can receive packet while in deep-sleep.

## Usage

See examples directory.

```
spi:
  clk_pin: GPIO23
  mosi_pin: GPIO22
  miso_pin: GPIO21

sx127x:
  cs_pin: GPIO5
  reset_pin: GPIO2
  dio0_pin: GPIO7
  on_packet:
    then:
      - logger.log:
          format: "Received %d bytes LoRa packet with SNR %.2f dB and RSSI %d dBm"
          args: [ "payload.size()", "snr", "rssi" ]
```

### Configurations variables

- **name** (*Optional*)
- **cs_pin** (*Required*, [Pin Schema](https://esphome.io/guides/configuration-types#config-pin-schema))
- **reset_pin** (*Required*, [Pin Schema](https://esphome.io/guides/configuration-types#config-pin-schema))
- **dio0_pin** (*Required*, [Pin Schema](https://esphome.io/guides/configuration-types#config-pin-schema))
- **rf_frequency** (*Optional*): Integer value between `137000000` and `1020000000`.
- **tx_power** (*Optional*): Integer value between `4` and `18`.
- **lora_bandwidth** (*Optional*): One of `7800`, `10400`, `15600`, `20800`, `31250`, `41700`, `62500`, `125000`, `250000`, `500000`.
- **lora_spreading_factor** (*Optional*): One of `SF6`, `SF7`, `SF8`, `SF9`, `SF10`, `SF11`, `SF12`.
- **lora_crc** (*Optional*, boolean): Defaults to `true`.
- **lora_codingrate** (*Optional*): One of `CR_4_5`, `CR_4_6`, `CR_4_7`, `CR_4_8`.
- **lora_preamble_length** (*Optional*, integer):  Default value: `12`. Minimal value: `6`. 
- **lora_syncword** (*Optional*, one byte)
- **lora_invert_iq** (*Optional*, boolean): Defaults to `false`.
- **opmod** (*Optional*): One of `sleep`, `standby`, `rx`. Operational mode when transceiver is not transmitting. Defaults to `standby`. Can be changed with `sx127x.set_opmod` action. Use `rx` to enable reception. Use `sleep` to minimize power consumption before deep-sleep.
- **queue_len** (*Optional*): Integer between `0` and `100`. Defaults to `10`. Length of the queue of pending message for transmission. If the queue is full, messages are dropped.

and parameters for [SPI devices](https://esphome.io/components/spi.html#generic-spi-device-component).

### Actions

- `sx127x.send`
  - **payload** (*Required*, string, bytes array or [templetable](https://esphome.io/automations/templates#config-templatable) returning an `std::vector<uint8_t>`)
- `sx127x.set_opmod`
  - **opmod** (Required): One of `sleep`, `standby`, `rx`.

### Automations

- `on_packet`: An automation to perform on LoRa packet reception. Usable variables:
  - `payload` (`std::vector<uint8_t>`)
  - `snr` (`float`)
  - `rssi` (`int16_t`)
```
sx127x:
  opmod: rx
  on_packet:
    then:
      - logger.log:
          format: "Received %d bytes LoRa packet with SNR %.2f dB and RSSI %d dBm"
          args: [ "payload.size()", "snr", "rssi" ]
```

### Conditions

- `sx127x.is_transmitting`: This condition checks if the transceiver is actually transmitting. You can wait the end of transmission:
```
- wait_until:
    condition:
      not:
        sx127x.is_transmitting:
```

## Alternatives

which also have been inspiration

- https://github.com/esphome/esphome/pull/7490 (SX127x)
- https://github.com/swoboda1337/sx126x-esphome (SX126x)
- https://github.com/PaulSchulz/esphome-lora-sx126x (SX126x)
