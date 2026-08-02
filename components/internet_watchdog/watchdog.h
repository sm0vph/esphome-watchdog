#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP
#include <WiFiUdp.h>
#endif

namespace esphome {
namespace internet_watchdog {

class WatchdogComponent : public Component {
 public:
  void setup() override;
  void loop() override;

  void set_latency_sensor(sensor::Sensor *value) { latency_sensor_ = value; }
  void set_restart_count_sensor(sensor::Sensor *value) { restart_count_sensor_ = value; }
  void set_status_sensor(text_sensor::TextSensor *value) { status_sensor_ = value; }
  void set_target_sensor(text_sensor::TextSensor *value) { target_sensor_ = value; }
  void set_failure_sensor(text_sensor::TextSensor *value) { failure_sensor_ = value; }
  void set_internet_ok_sensor(binary_sensor::BinarySensor *value) { internet_ok_sensor_ = value; }
  void set_relay(switch_::Switch *value) { relay_ = value; }
  void set_dns_servers(const std::vector<std::string> &value) { dns_servers_ = value; }
  void set_check_domain(const std::string &value) { check_domain_ = value; }
  void set_startup_grace_time(uint32_t value) { startup_grace_time_ = value; }
  void set_power_off_time(uint32_t value) { power_off_time_ = value; }
  void set_boot_wait_time(uint32_t value) { boot_wait_time_ = value; }
  void set_dns_timeout(uint32_t value) { dns_timeout_ = value; }
  void set_backoff_initial_time(uint32_t value) { backoff_initial_time_ = value; }
  void set_backoff_max_time(uint32_t value) { backoff_max_time_ = value; }
  void set_backoff_multiplier(float value) { backoff_multiplier_ = value; }
  void set_maintenance_switch(switch_::Switch *value) { maintenance_switch_ = value; }
  void set_maintenance_timeout(uint32_t value) { maintenance_timeout_ = value; }
  void set_check_interval(uint32_t value) { check_interval_ = value; }
  void set_reboot_backoff(bool value) { reboot_backoff_ = value; }
  void set_failure_threshold(uint32_t value) { failure_threshold_ = value; }
  void set_network_connect_timeout(uint32_t value) { network_connect_timeout_ = value; }
  void set_device_reboot_before_power_cycle(bool value) { device_reboot_before_power_cycle_ = value; }

 protected:
  enum class DnsState { IDLE, WAITING };
  enum class RestartState { IDLE, BACKOFF_WAIT, POWER_OFF_WAIT, BOOT_WAIT };

  static constexpr size_t DNS_BUFFER_SIZE = 512;
  static constexpr uint16_t DNS_PORT = 53;

  bool open_dns_socket_();
  bool start_dns_query_();
  bool build_dns_query_(std::array<uint8_t, DNS_BUFFER_SIZE> &buffer, size_t &length) const;
  bool process_dns_response_();
  bool valid_dns_response_(const uint8_t *data, size_t length) const;
  void try_next_dns_server_();
  void finish_check_success_(uint32_t latency);
  void finish_check_failure_();
  void reset_dns_query_();
  void handle_network_unavailable_();
  void handle_auto_restart_request_();
  void update_maintenance_timeout_();
  void power_cycle_();
  uint32_t calculate_backoff_() const;

  void publish_status_(const char *value);
  void publish_target_(const char *value);
  void publish_failure_(const char *value);
  void publish_internet_ok_(bool value);

#ifdef USE_SOCKET_IMPL_LWIP_TCP
  WiFiUDP dns_udp_;
  bool dns_udp_started_{false};
#else
  std::unique_ptr<socket::Socket> dns_socket_;
#endif
  std::vector<std::string> dns_servers_;
  std::string check_domain_;
  size_t current_dns_server_{0};
  DnsState dns_state_{DnsState::IDLE};
  uint16_t dns_transaction_id_{0};
  uint32_t dns_query_started_at_{0};

  bool startup_delay_started_{false};
  bool startup_completed_{false};
  uint32_t startup_delay_started_at_{0};
  uint32_t last_check_time_{0};
  uint32_t next_network_failure_check_{0};

  uint32_t startup_grace_time_{30000};
  uint32_t power_off_time_{20000};
  uint32_t boot_wait_time_{180000};
  uint32_t dns_timeout_{2000};
  uint32_t check_interval_{60000};
  uint32_t network_connect_timeout_{420000};
  uint32_t failure_threshold_{2};
  uint32_t consecutive_failure_rounds_{0};

  uint32_t backoff_initial_time_{300000};
  uint32_t backoff_max_time_{3600000};
  float backoff_multiplier_{2.0f};
  bool reboot_backoff_{true};
  uint32_t restart_attempts_{0};
  RestartState restart_state_{RestartState::IDLE};
  uint32_t restart_timer_{0};
  uint32_t next_restart_allowed_{0};

  bool device_reboot_before_power_cycle_{true};
  bool device_reboot_attempted_{false};
  ESPPreferenceObject device_reboot_pref_;

  switch_::Switch *maintenance_switch_{nullptr};
  uint32_t maintenance_timeout_{0};
  uint32_t maintenance_started_at_{0};
  bool maintenance_was_active_{false};

  sensor::Sensor *latency_sensor_{nullptr};
  sensor::Sensor *restart_count_sensor_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr};
  text_sensor::TextSensor *target_sensor_{nullptr};
  text_sensor::TextSensor *failure_sensor_{nullptr};
  binary_sensor::BinarySensor *internet_ok_sensor_{nullptr};
  switch_::Switch *relay_{nullptr};
};

}  // namespace internet_watchdog
}  // namespace esphome
