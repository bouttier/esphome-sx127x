#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include <vector>

#include "sx127x_component.h"

namespace esphome {
namespace sx127x {

template <typename... Ts>
class SX127XSendAction : public Action<Ts...>,
                         public Parented<SX127XComponent> {
public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, payload);

  void play(Ts... x) override {
    this->parent_->send(this->payload_.value(x...));
  }

protected:
  bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> payload_func_{};
  std::vector<uint8_t> payload_static_{};
};

template <typename... Ts>
class SX127XSetOpmodAction : public Action<Ts...>,
                             public Parented<SX127XComponent> {
public:
  TEMPLATABLE_VALUE(sx127x_mode_t, opmod)

  void play(Ts... x) override {
    this->parent_->change_opmod(this->opmod_.value(x...));
  }
};

class SX127XRecvTrigger : public Trigger<std::vector<uint8_t>, float, int16_t> {
public:
  explicit SX127XRecvTrigger(SX127XComponent *parent) {
    parent->add_on_packet_callback(
        [this](const std::vector<uint8_t> &payload, float snr, int16_t rssi) {
          this->trigger(payload, snr, rssi);
        });
  }
};

template <typename... Ts>
class SX127XIsTransmittingCondition : public Condition<Ts...>,
                                      public Parented<SX127XComponent> {
public:
  bool check(Ts... x) override { return this->parent_->is_transmitting(); }
};

} // namespace sx127x
} // namespace esphome
