#include "esphome.h"

#include "sx127x_component.h"

#include "esp_sleep.h"
#ifdef USE_ESP32
#include <freertos/task.h>
#endif

static const char *TAG = "sx127x";

namespace esphome {
namespace sx127x {

void SX127XComponent::setup() {
  ESP_LOGCONFIG(TAG, "LoRa SX127X Setup (SX1278)");

#ifdef USE_ESP32
  bool reconfigure = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED;
#else
  bool reconfigure = true;
#endif

  // We set state HIGH before pin setup to avoid accidental reset
  // This is especialy useful when waking-up from deep-sleep
  this->reset_pin_->digital_write(true);
  this->reset_pin_->setup();
  if (dio0_pin_) {
    this->dio0_pin_->setup();
  }

  this->spi_setup();

  transmitting_ = false;
  this->packets_rx_zero();
  this->packets_tx_zero();

  if (reconfigure) {
    this->reset();
  }

  ESP_LOGCONFIG(TAG, "sx127x_create");
  if (sx127x_create(this, &device_)) {
    this->mark_failed();
    return;
  }
  sx127x_set_user_context(this, &device_);

  // Interrupts
  ESP_LOGCONFIG(TAG, "attach_interrupt on dio0 pin");
  sx127x_rx_set_callback(&SX127XComponent::_rx_callback, &device_);
  sx127x_tx_set_callback(&SX127XComponent::_tx_callback, &device_);
  if (dio0_pin_) {
    this->dio0_pin_->attach_interrupt(SX127XComponent::dio0_intr, this,
                                      gpio::INTERRUPT_RISING_EDGE);
    // If waking-up from deep-sleep, check any received message
    dio0_flag_ = !reconfigure;
  } else {
    dio0_flag_ = true;
  }

  if (reconfigure) {
    this->configure();
  }

  if (this->update_opmod()) {
    this->mark_failed();
    return;
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
  ESP_LOGCONFIG(TAG, "sx127x_lora_reset_fifo()");
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
  ESP_LOGCONFIG(TAG, "sx127x_lora_set_modem_config_2(spreading_factor=%d)",
                this->lora_spreading_factor_);
  if (sx127x_lora_set_modem_config_2(this->lora_spreading_factor_, &device_)) {
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "sx127x_lora_set_syncword(0x%x)", this->lora_syncword_);
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

  // TX
  ESP_LOGCONFIG(TAG, "sx127x_tx_set_pa_config(pa_pin=%d, tx_power=%d)",
                this->pa_pin_, this->tx_power_);
  if (sx127x_tx_set_pa_config(pa_pin_, tx_power_, &device_)) {
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

void IRAM_ATTR SX127XComponent::dio0_intr(SX127XComponent *sx127x) {
  sx127x->dio0_flag_ = true;
}

void SX127XComponent::loop() {
  if (dio0_flag_) {
    if (dio0_pin_)
      dio0_flag_ = false;
    ESP_LOGD(TAG, "sx127x_handle_interrupt");
    sx127x_handle_interrupt(&device_);
  }
}

void SX127XComponent::send(const std::vector<uint8_t> &payload) {
  if (this->is_failed()) {
    ESP_LOGD(TAG, "send: cancelled due to failed state");
    return;
  }
  if (this->is_transmitting() || !this->is_ready()) {
    if (this->tx_queue_.size() >= this->queue_len_) {
      ESP_LOGE(TAG, "send: queue is full, message dropped");
      return;
    }
    this->tx_queue_.push_back(payload);
    ESP_LOGD(TAG, "Enqueued %d-bytes LoRa message (pending: %d messages)",
             payload.size(), tx_queue_.size());
    return;
  }

  xmit(payload);
}

void SX127XComponent::send_next() {
  if (this->tx_queue_.size()) {
    auto payload = this->tx_queue_.front();
    this->tx_queue_.pop_front();
    this->xmit(payload);
  } else {
    this->transmitting_ = false;
    this->update_opmod();
  }
}

void SX127XComponent::xmit(const std::vector<uint8_t> &payload) {
  this->transmitting_ = true;
  ESP_LOGD(TAG, "Send %d-bytes LoRa message", payload.size());
  if (sx127x_lora_tx_set_for_transmission(payload.data(), payload.size(),
                                          &device_)) {
    ESP_LOGE(TAG, "sx127x_lora_tx_set_for_transmission failed");
    return;
  }
  ESP_LOGD(TAG, "sx127x_set_opmod(TX)");
  if (sx127x_set_opmod(SX127x_MODE_TX, SX127x_MODULATION_LORA, &device_)) {
    ESP_LOGE(TAG, "sx127x_set_opmod failed");
    return;
  }
  auto f = std::bind(&SX127XComponent::xmit_timeout, this);
  this->set_timeout("timeout", timeout_ms_, f);
}

void SX127XComponent::xmit_timeout() {
  ESP_LOGE(TAG, "TX timeout");
  this->send_next();
}

void SX127XComponent::tx_callback() {
  ESP_LOGD(TAG, "TX done");
  this->cancel_timeout("timeout");
  this->packets_tx_incr();
  this->send_next();
}

void SX127XComponent::rx_callback(std::vector<uint8_t> payload) {
  int16_t rssi;
  sx127x_rx_get_packet_rssi(&device_, &rssi);
  float snr = std::numeric_limits<float>::quiet_NaN();
  sx127x_lora_rx_get_packet_snr(&device_, &snr);
  ESP_LOGD(TAG, "RX %d bytes packet with SNR %.2f dB and RSSI %d dBm",
           payload.size(), snr, rssi);
  this->update_opmod();
  this->packets_rx_incr();
  this->rx_callback_.call(payload, snr, rssi);
}

void SX127XComponent::change_opmod(sx127x_mode_t opmod) {
  if (this->is_failed())
    return;
  ESP_LOGD(TAG, "change opmod: %s > %s", opmod_to_string(this->opmod_).c_str(),
           opmod_to_string(opmod).c_str());
  if (this->opmod_ == opmod)
    return;
  this->set_opmod(opmod);
  if (this->is_ready())
    this->update_opmod();
}

int SX127XComponent::update_opmod() {
  int ret = ESP_OK;

  if (is_transmitting())
    return ret;

  ESP_LOGD(TAG, "sx127x_set_opmod(%s)", opmod_to_string(this->opmod_).c_str());
  if ((ret =
           sx127x_set_opmod(this->opmod_, SX127x_MODULATION_LORA, &device_))) {
    ESP_LOGE(TAG, "sx127x_set_opmod failed");
  }
  return ret;
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
  ESP_LOGCONFIG(TAG, "  Opmod:               %8s",
                opmod_to_string(this->opmod_).c_str());
  ESP_LOGCONFIG(TAG, "  TX queue length:          %3d", this->queue_len_);
}

void SX127XComponent::_tx_callback(sx127x_t *device) {
  void *context = sx127x_get_user_context(device);
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(context);
  sx127x->tx_callback();
}

void SX127XComponent::_rx_callback(sx127x_t *device, uint8_t *payload,
                                   uint16_t len) {
  void *context = sx127x_get_user_context(device);
  SX127XComponent *sx127x = static_cast<SX127XComponent *>(context);
  sx127x->rx_callback(std::vector<uint8_t>(payload, payload + len));
}

std::string opmod_to_string(sx127x_mode_t opmod) {
  switch (opmod) {
  case SX127x_MODE_SLEEP:
    return "sleep";
  case SX127x_MODE_STANDBY:
    return "standby";
  case SX127x_MODE_TX:
    return "tx";
  case SX127x_MODE_RX_CONT:
    return "rx";
  default:
    return std::to_string(opmod);
  }
}

} // namespace sx127x
} // namespace esphome
