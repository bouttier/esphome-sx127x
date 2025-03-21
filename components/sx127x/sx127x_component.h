#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include <queue>
#include <vector>

#include <sx127x.h>

namespace esphome {
namespace sx127x {

class SX127XComponent
    : public Component,
      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                            spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {

public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void teardown();

  static void dio0_intr(SX127XComponent *arg);
  static void handle_interrupt_task(void *arg);
  static void _tx_callback(sx127x_t *device);
  void tx_callback();
  static void _rx_callback(sx127x_t *device, uint8_t *payload, uint16_t len);
  void rx_callback(std::vector<uint8_t> payload);

  // Actions
  void send(const std::vector<uint8_t> &payload);
  void change_opmod(sx127x_mode_t opmod);

  // Triggers
  void add_on_packet_callback(
      std::function<void(std::vector<uint8_t>, float, int16_t)> &&callback) {
    this->rx_callback_.add(std::move(callback));
  }

  // Parameters
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_dio0_pin(InternalGPIOPin *dio0_pin) { this->dio0_pin_ = dio0_pin; }
  void set_rf_frequency(uint32_t rf_frequency) {
    this->rf_frequency_ = rf_frequency;
  }
  void set_tx_power(uint8_t tx_power) { this->tx_power_ = tx_power; }
  void set_pa_pin(sx127x_pa_pin_t pa_pin) { this->pa_pin_ = pa_pin; }
  void set_lora_bandwidth(sx127x_bw_t lora_bandwidth) {
    this->lora_bandwidth_ = lora_bandwidth;
  }
  void set_lora_spreading_factor(sx127x_sf_t lora_spreading_factor) {
    this->lora_spreading_factor_ = lora_spreading_factor;
  }
  void set_lora_enable_crc(bool lora_enable_crc) {
    this->lora_enable_crc_ = lora_enable_crc;
  }
  void set_lora_codingrate(sx127x_cr_t lora_codingrate) {
    this->lora_codingrate_ = lora_codingrate;
  }
  void set_lora_preamble_length(uint8_t lora_preamble_length) {
    this->lora_preamble_length_ = lora_preamble_length;
  }
  void set_lora_syncword(uint8_t lora_syncword) {
    this->lora_syncword_ = lora_syncword;
  }
  void set_lora_invert_iq(bool lora_invert_iq) {
    this->lora_invert_iq_ = lora_invert_iq;
  }
  void set_opmod(sx127x_mode_t opmod) { this->opmod_ = opmod; }
  void set_queue_len(unsigned int queue_len) { this->queue_len_ = queue_len; }

  // Conditions
  bool is_transmitting() { return this->transmitting_; }

  void packets_rx_zero(void) { this->lora_packets_rx_ = 0; }
  void packets_rx_incr(void) { this->lora_packets_rx_++; }
  uint16_t packets_rx(void) { return lora_packets_rx_; }

  void packets_tx_zero(void) { this->lora_packets_tx_ = 0; }
  void packets_tx_incr(void) { this->lora_packets_tx_++; }
  uint16_t packets_tx(void) { return lora_packets_tx_; }

protected:
  GPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *dio0_pin_{nullptr};

  // lora configuration
  uint32_t rf_frequency_;
  uint8_t tx_power_;
  sx127x_pa_pin_t pa_pin_;
  sx127x_bw_t lora_bandwidth_;
  sx127x_sf_t lora_spreading_factor_;
  bool lora_enable_crc_;
  sx127x_cr_t lora_codingrate_;
  uint8_t lora_preamble_length_;
  uint8_t lora_syncword_;
  bool lora_invert_iq_;
  sx127x_mode_t opmod_;
  unsigned int queue_len_;

  bool dio0_flag_;
  bool transmitting_;

  uint16_t lora_packets_rx_;
  uint16_t lora_packets_tx_;

  sx127x_t device_;
  TaskHandle_t handle_interrupt_;
  CallbackManager<void(std::vector<uint8_t>, float, int16_t)> rx_callback_;
  std::deque<std::vector<uint8_t>> tx_queue_;

  void reset();
  void configure();
  int update_opmod();
  void send_next();
  void xmit(const std::vector<uint8_t> &payload);
  void xmit_timeout();
}; // class SX127XComponent

std::string opmod_to_string(sx127x_mode_t opmod);

} // namespace sx127x
} // namespace esphome
