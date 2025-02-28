#include "sx127x_component.h"

using esphome::sx127x::SX127XComponent;

extern "C" {

int sx127x_spi_read_registers(int reg, void *spi_device, size_t data_length,
                              uint32_t *result) {
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(spi_device);

  if (data_length == 0 || data_length > 4) {
    return ESP_ERR_INVALID_ARG;
  }
  *result = 0;
  sx127x->enable();
  sx127x->write_byte(reg & 0x7F);
  for (int i = 0; i < data_length; i++) {
    *result = ((*result) << 8);
    *result = (*result) + sx127x->read_byte();
  }
  sx127x->disable();
  return ESP_OK;
}

int sx127x_spi_read_buffer(int reg, uint8_t *buffer, size_t buffer_length,
                           void *spi_device) {
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(spi_device);
  sx127x->enable();
  sx127x->write_byte(reg & 0x7F);
  sx127x->read_array(buffer, buffer_length);
  sx127x->disable();
  return ESP_OK;
}

int sx127x_spi_write_register(int reg, const uint8_t *data, size_t data_length,
                              void *spi_device) {
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(spi_device);

  if (data_length == 0 || data_length > 4) {
    return ESP_ERR_INVALID_ARG;
  }
  sx127x->enable();
  sx127x->write_byte(reg | 0x80);
  for (int i = 0; i < data_length; i++) {
    sx127x->write_byte(data[i]);
  }
  sx127x->disable();
  return ESP_OK;
}

int sx127x_spi_write_buffer(int reg, const uint8_t *buffer,
                            size_t buffer_length, void *spi_device) {
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(spi_device);
  sx127x->enable();
  sx127x->write_byte(reg | 0x80);
  sx127x->write_array(buffer, buffer_length);
  sx127x->disable();
  return ESP_OK;
}

} // extern "C"
