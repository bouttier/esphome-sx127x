#include "esphome.h"

#include "sx127x_component.h"

#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>

static const char *TAG = "sx127x";

namespace esphome {
namespace sx127x {

void SX127XComponent::setup() {
  ESP_LOGCONFIG(TAG, "LoRa SX127X Setup (SX1278)");

  // We set state HIGH before pin setup to avoid accidental reset
  // This is especialy useful when waking-up from deep-sleep
  this->reset_pin_->digital_write(true);
  this->reset_pin_->setup();
  this->dio0_pin_->setup();

  this->spi_setup();

  // Radio Statistics
  this->packets_rx_zero();
  this->packets_tx_zero();

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    this->reset();
  }

  ESP_LOGCONFIG(TAG, "sx127x_create");
  if (sx127x_create(this, &device_)) {
    this->mark_failed();
    return;
  }
  sx127x_set_user_context(this, &device_);

  // Interrupts
  BaseType_t task_code =
      xTaskCreatePinnedToCore(handle_interrupt_task, "sx127x", 8196, this, 2,
                              &handle_interrupt_, xPortGetCoreID());
  if (task_code != pdPASS) {
    ESP_LOGE(TAG, "can't create task %d", task_code);
    this->mark_failed();
    return;
  }

  sx127x_rx_set_callback(&SX127XComponent::_rx_callback, &device_);
  sx127x_tx_set_callback(&SX127XComponent::_tx_callback, &device_);

  ESP_LOGCONFIG(TAG, "attach_interrupt on dio0 pin");
  this->dio0_pin_->attach_interrupt(SX127XComponent::dio0_intr, this,
                                    gpio::INTERRUPT_RISING_EDGE);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    this->configure();
  }
}

void SX127XComponent::teardown() { this->spi_teardown(); }

void SX127XComponent::reset() {
  this->reset_pin_->digital_write(false);
  delay(1); // datasheet say at least 100μs
  this->reset_pin_->digital_write(true);
  delay(8); // datasheet say at least 5ms
  ESP_LOGD(TAG, "sx127x was reset");
}

void SX127XComponent::configure() {
  ESP_LOGCONFIG(TAG, "sx127x_set_opmod(MODE_SLEEP, MODULATION_LORA)");
  if (sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, &device_)) {
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "sx127x_set_frequency(%d)", this->rf_frequency_);
  if (sx127x_set_frequency(this->rf_frequency_, &device_)) {
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "sx127x_lora_reset_fifo");
  if (sx127x_lora_reset_fifo(&device_)) {
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "sx127x_lora_set_bandwidth(%d)", this->lora_bandwidth_);
  if (sx127x_lora_set_bandwidth(this->lora_bandwidth_, &device_)) {
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "sx127x_lora_set_implicit_header(NULL)");
  if (sx127x_lora_set_implicit_header(NULL, &device_)) {
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "sx127x_lora_set_modem_config_2(%d)",
                this->lora_spreading_factor_);
  if (sx127x_lora_set_modem_config_2(this->lora_spreading_factor_, &device_)) {
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "sx127x_lora_set_modem_config_2(0x%x)",
                this->lora_syncword_);
  if (sx127x_lora_set_syncword(this->lora_syncword_, &device_)) {
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "sx127x_set_preamble_length(%d)",
                this->lora_preamble_length_);
  if (sx127x_set_preamble_length(this->lora_preamble_length_, &device_)) {
    this->mark_failed();
    return;
  }

  // RX
  if (this->enable_rx_) {
    ESP_LOGCONFIG(TAG, "sx127x_set_opmod(MODE_RX_CONT, MODULATION_LORA)");
    if (sx127x_set_opmod(SX127x_MODE_RX_CONT, SX127x_MODULATION_LORA,
                         &device_)) {
      this->mark_failed();
      return;
    }
  }

  // TX
  ESP_LOGCONFIG(TAG, "sx127x_tx_set_pa_config(PA_PIN_BOOST, %d)",
                this->tx_power_);
  if (sx127x_tx_set_pa_config(SX127x_PA_PIN_BOOST, tx_power_, &device_)) {
    this->mark_failed();
    return;
  }
  sx127x_tx_header_t header = {
      .enable_crc = this->lora_enable_crc_,
      .coding_rate = this->lora_codingrate_,
  };
  ESP_LOGCONFIG(
      TAG, "sx127x_lora_tx_set_explicit_header(enable_crc=%d, coding_rate=%d)",
      header.enable_crc, header.coding_rate);
  if (sx127x_lora_tx_set_explicit_header(&header, &device_)) {
    this->mark_failed();
    return;
  }
}

void SX127XComponent::loop() {
  static bool first_run = true;
  if (first_run) {
    first_run = false;
    // If waking-up from deep-sleep, check if there is any pending message
    // We trigger handle_interrup here to be sure that others components are
    // available as they may be used in actions.
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
      xTaskNotifyGive(handle_interrupt_);
    }
  }
}

void SX127XComponent::send(const uint8_t *payload, uint8_t len) {
  if (this->is_failed()) {
    ESP_LOGD(TAG, "send: cancelled due to failed state");
    return;
  }

  ESP_LOGD(TAG, "Send %d-bytes LoRa message", len);
  if (sx127x_lora_tx_set_for_transmission(payload, len, &device_)) {
    ESP_LOGE(TAG, "sx127x_lora_tx_set_for_transmission failed");
    return;
  }
  if (sx127x_set_opmod(SX127x_MODE_TX, SX127x_MODULATION_LORA, &device_)) {
    ESP_LOGE(TAG, "sx127x_set_opmod failed");
    return;
  }
}

void IRAM_ATTR SX127XComponent::dio0_intr(SX127XComponent *sx127x) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(sx127x->handle_interrupt_, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void SX127XComponent::handle_interrupt_task(void *arg) {
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(arg);
  while (1) {
    if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {
      ESP_LOGD(TAG, "sx127x_handle_interrupt");
      sx127x_handle_interrupt(&sx127x->device_);
      if (sx127x->enable_rx_) {
        ESP_LOGCONFIG(TAG, "sx127x_set_opmod(MODE_RX_CONT, MODULATION_LORA)");
        if (sx127x_set_opmod(SX127x_MODE_RX_CONT, SX127x_MODULATION_LORA,
                             &sx127x->device_)) {
          ESP_LOGE(TAG, "failed to go back to rx mode");
          return;
        }
      } else {
        ESP_LOGCONFIG(TAG, "sx127x_set_opmod(MODE_SLEEP, MODULATION_LORA)");
        if (sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA,
                             &sx127x->device_)) {
          ESP_LOGE(TAG, "failed to go back to sleep mode");
          return;
        }
      }
    }
  }
}

void SX127XComponent::_tx_callback(sx127x_t *device) {
  void *context = sx127x_get_user_context(device);
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(context);
  sx127x->tx_callback();
}

void SX127XComponent::tx_callback() {
  this->packets_tx_incr();
  ESP_LOGD(TAG, "TX done");
}

void SX127XComponent::_rx_callback(sx127x_t *device, uint8_t *payload,
                                   uint16_t len) {
  void *context = sx127x_get_user_context(device);
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(context);
  sx127x->rx_callback(std::vector<uint8_t>(payload, payload + len));
}

void SX127XComponent::rx_callback(std::vector<uint8_t> payload) {
  this->packets_rx_incr();
  ESP_LOGD(TAG, "RX %d bytes packet", payload.size());
  this->callback_.call(payload);
}

void SX127XComponent::enable_rx() {
  if (this->is_failed())
    return;
  if (this->enable_rx_)
    return;
  this->enable_rx_ = true;
  if (sx127x_set_opmod(SX127x_MODE_RX_CONT, SX127x_MODULATION_LORA,
                       &device_)) { // XXX no active TX?
    ESP_LOGE(TAG, "failed to go back to rx mode");
    return;
  }
}
void SX127XComponent::disable_rx() {
  if (this->is_failed())
    return;
  if (!this->enable_rx_)
    return;
  this->enable_rx_ = false;
  if (sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA,
                       &device_)) { // XXX no active TX?
    ESP_LOGE(TAG, "failed to go back to sleep mode");
    return;
  }
}

void SX127XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SX126X Config");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DIO0 Pin: ", this->dio0_pin_);
  ESP_LOGCONFIG(TAG, "LoRa Configuration");
  ESP_LOGCONFIG(TAG, "  Frequency:          %9d Hz", this->rf_frequency_);
  ESP_LOGCONFIG(TAG, "  TX Power:                 %3d dBm", this->tx_power_);
  switch (this->lora_bandwidth_) {
  case SX127x_BW_7800:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:           7.8 kHz");
    break;
  case SX127x_BW_10400:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:          10.4 kHz");
    break;
  case SX127x_BW_15600:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:          15.6 kHz");
    break;
  case SX127x_BW_20800:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:          20.8 kHz");
    break;
  case SX127x_BW_31250:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:         31.25 kHz");
    break;
  case SX127x_BW_41700:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:          41.7 kHz");
    break;
  case SX127x_BW_62500:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:          62.5 kHz");
    break;
  case SX127x_BW_125000:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:           125 kHz");
    break;
  case SX127x_BW_250000:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:           250 kHz");
    break;
  case SX127x_BW_500000:
    ESP_LOGCONFIG(TAG, "  LoRa Bandwidth:           500 kHz");
    break;
  }
  switch (this->lora_spreading_factor_) {
  case SX127x_SF_6:
    ESP_LOGCONFIG(TAG, "  LoRa Spreading Factor:    SF6");
    break;
  case SX127x_SF_7:
    ESP_LOGCONFIG(TAG, "  LoRa Spreading Factor:    SF7");
    break;
  case SX127x_SF_8:
    ESP_LOGCONFIG(TAG, "  LoRa Spreading Factor:    SF8");
    break;
  case SX127x_SF_9:
    ESP_LOGCONFIG(TAG, "  LoRa Spreading Factor:    SF9");
    break;
  case SX127x_SF_10:
    ESP_LOGCONFIG(TAG, "  LoRa Spreading Factor:   SF10");
    break;
  case SX127x_SF_11:
    ESP_LOGCONFIG(TAG, "  LoRa Spreading Factor:   SF11");
    break;
  case SX127x_SF_12:
    ESP_LOGCONFIG(TAG, "  LoRa Spreading Factor:   SF12");
    break;
  }
  switch (this->lora_codingrate_) {
  case SX127x_CR_4_5:
    ESP_LOGCONFIG(TAG, "  LoRa Coding Rate:         4/5");
    break;
  case SX127x_CR_4_6:
    ESP_LOGCONFIG(TAG, "  LoRa Coding Rate:         4/6");
    break;
  case SX127x_CR_4_7:
    ESP_LOGCONFIG(TAG, "  LoRa Coding Rate:         4/7");
    break;
  case SX127x_CR_4_8:
    ESP_LOGCONFIG(TAG, "  LoRa Coding Rate:         4/8");
    break;
  }
  ESP_LOGCONFIG(TAG, "  CRC:                      %3s",
                YESNO(this->lora_enable_crc_));
  ESP_LOGCONFIG(TAG, "  LoRa Preamble Length:     %3d",
                this->lora_preamble_length_);
  ESP_LOGCONFIG(TAG, "  LoRa Syncword:           0x%2X", this->lora_syncword_);
  ESP_LOGCONFIG(TAG, "  LoRa Invert IQ:           %3s",
                YESNO(this->lora_invert_iq_));
  ESP_LOGCONFIG(TAG, "  Start RX:                 %3s",
                YESNO(this->enable_rx_));
}

} // namespace sx127x
} // namespace esphome
