#include "watchdog.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace internet_watchdog {

static const char *const TAG = "internet_watchdog";

void WatchdogComponent::publish_status_(const char *value) {
  if (status_sensor_ != nullptr)
    status_sensor_->publish_state(value);
}

void WatchdogComponent::publish_target_(const char *value) {
  if (target_sensor_ != nullptr)
    target_sensor_->publish_state(value);
}

void WatchdogComponent::publish_failure_(const char *value) {
  if (failure_sensor_ != nullptr)
    failure_sensor_->publish_state(value);
}

void WatchdogComponent::publish_internet_ok_(bool value) {
  if (internet_ok_sensor_ != nullptr)
    internet_ok_sensor_->publish_state(value);
}

bool WatchdogComponent::open_dns_socket_() {
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  if (dns_udp_started_)
    return true;
  dns_udp_started_ = dns_udp_.begin(0) != 0;
  if (!dns_udp_started_)
    ESP_LOGE(TAG, "Could not create DNS UDP transport");
  return dns_udp_started_;
#else
  if (dns_socket_ != nullptr)
    return true;

  dns_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (dns_socket_ == nullptr) {
    ESP_LOGE(TAG, "Could not create DNS socket: errno %d", errno);
    return false;
  }

  if (dns_socket_->setblocking(false) != 0) {
    ESP_LOGE(TAG, "Could not make DNS socket non-blocking: errno %d", errno);
    dns_socket_.reset();
    return false;
  }

  return true;
#endif
}

bool WatchdogComponent::build_dns_query_(std::array<uint8_t, DNS_BUFFER_SIZE> &buffer, size_t &length) const {
  buffer.fill(0);
  buffer[0] = static_cast<uint8_t>(dns_transaction_id_ >> 8);
  buffer[1] = static_cast<uint8_t>(dns_transaction_id_ & 0xFF);
  buffer[2] = 0x01;  // Recursion desired.
  buffer[5] = 0x01;  // One question.

  size_t offset = 12;
  size_t label_start = 0;
  while (label_start < check_domain_.size()) {
    const size_t dot = check_domain_.find('.', label_start);
    const size_t label_end = dot == std::string::npos ? check_domain_.size() : dot;
    const size_t label_length = label_end - label_start;
    if (label_length == 0 || label_length > 63 || offset + label_length + 1 >= buffer.size())
      return false;

    buffer[offset++] = static_cast<uint8_t>(label_length);
    std::memcpy(buffer.data() + offset, check_domain_.data() + label_start, label_length);
    offset += label_length;
    if (dot == std::string::npos)
      break;
    label_start = dot + 1;
  }

  if (offset + 5 > buffer.size())
    return false;
  buffer[offset++] = 0;
  buffer[offset++] = 0;
  buffer[offset++] = 1;  // A record.
  buffer[offset++] = 0;
  buffer[offset++] = 1;  // Internet class.
  length = offset;
  return true;
}

bool WatchdogComponent::start_dns_query_() {
  if (current_dns_server_ >= dns_servers_.size() || !open_dns_socket_())
    return false;

  dns_transaction_id_ = static_cast<uint16_t>(millis() ^ ((current_dns_server_ + 1) * 0x9E37U));
  if (dns_transaction_id_ == 0)
    dns_transaction_id_ = 1;

  std::array<uint8_t, DNS_BUFFER_SIZE> query{};
  size_t query_length = 0;
  if (!build_dns_query_(query, query_length)) {
    ESP_LOGE(TAG, "Invalid DNS check domain: %s", check_domain_.c_str());
    return false;
  }

#ifdef USE_SOCKET_IMPL_LWIP_TCP
  IPAddress destination;
  if (!destination.fromString(dns_servers_[current_dns_server_].c_str()) ||
      dns_udp_.beginPacket(destination, DNS_PORT) == 0 ||
      dns_udp_.write(query.data(), query_length) != query_length || dns_udp_.endPacket() == 0) {
    ESP_LOGW(TAG, "Failed to query DNS server %s", dns_servers_[current_dns_server_].c_str());
    return false;
  }
#else
  struct sockaddr_storage destination {};
  const socklen_t destination_length = socket::set_sockaddr(
      reinterpret_cast<struct sockaddr *>(&destination), sizeof(destination),
      dns_servers_[current_dns_server_], DNS_PORT);
  if (destination_length == 0) {
    ESP_LOGW(TAG, "Invalid DNS server address: %s", dns_servers_[current_dns_server_].c_str());
    return false;
  }

  const ssize_t sent = dns_socket_->sendto(
      query.data(), query_length, 0, reinterpret_cast<struct sockaddr *>(&destination), destination_length);
  if (sent != static_cast<ssize_t>(query_length)) {
    ESP_LOGW(TAG, "Failed to query DNS server %s: errno %d", dns_servers_[current_dns_server_].c_str(), errno);
    return false;
  }
#endif

  dns_query_started_at_ = millis();
  dns_state_ = DnsState::WAITING;
  publish_status_("Checking internet");
  publish_target_(dns_servers_[current_dns_server_].c_str());
  ESP_LOGD(TAG, "Querying %s via DNS server %s", check_domain_.c_str(), dns_servers_[current_dns_server_].c_str());
  return true;
}

bool WatchdogComponent::valid_dns_response_(const uint8_t *data, size_t length) const {
  if (length < 12)
    return false;

  const uint16_t transaction_id = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t flags = (static_cast<uint16_t>(data[2]) << 8) | data[3];
  const uint16_t question_count = (static_cast<uint16_t>(data[4]) << 8) | data[5];
  const uint16_t answer_count = (static_cast<uint16_t>(data[6]) << 8) | data[7];

  const bool is_response = (flags & 0x8000U) != 0;
  const bool standard_query = (flags & 0x7800U) == 0;
  const uint8_t response_code = static_cast<uint8_t>(flags & 0x000FU);
  return transaction_id == dns_transaction_id_ && is_response && standard_query && response_code == 0 &&
         question_count == 1 && answer_count > 0;
}

bool WatchdogComponent::process_dns_response_() {
  std::array<uint8_t, DNS_BUFFER_SIZE> response{};
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  for (;;) {
    const int packet_size = dns_udp_.parsePacket();
    if (packet_size <= 0)
      return false;
    const int received = dns_udp_.read(response.data(), response.size());
    if (received > 0 && valid_dns_response_(response.data(), static_cast<size_t>(received)))
      return true;
  }
#else
  struct sockaddr_storage source {};
  socklen_t source_length = sizeof(source);

  for (;;) {
    const ssize_t received = dns_socket_->recvfrom(
        response.data(), response.size(), reinterpret_cast<struct sockaddr *>(&source), &source_length);
    if (received < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        ESP_LOGW(TAG, "DNS receive failed: errno %d", errno);
      return false;
    }
    if (received == 0)
      return false;
    if (valid_dns_response_(response.data(), static_cast<size_t>(received)))
      return true;
    source_length = sizeof(source);
  }
#endif
}

void WatchdogComponent::reset_dns_query_() {
  dns_state_ = DnsState::IDLE;
  current_dns_server_ = 0;
}

void WatchdogComponent::finish_check_success_(uint32_t latency) {
  ESP_LOGI(TAG, "Internet OK via DNS server %s (%u ms)", dns_servers_[current_dns_server_].c_str(), latency);
  publish_status_("OK");
  publish_failure_("None");
  publish_internet_ok_(true);
  if (latency_sensor_ != nullptr)
    latency_sensor_->publish_state(latency);

  consecutive_failure_rounds_ = 0;
  restart_attempts_ = 0;
  next_restart_allowed_ = 0;
  if (restart_count_sensor_ != nullptr)
    restart_count_sensor_->publish_state(0);

  last_check_time_ = millis();
  reset_dns_query_();
}

void WatchdogComponent::try_next_dns_server_() {
  dns_state_ = DnsState::IDLE;
  current_dns_server_++;
  if (current_dns_server_ >= dns_servers_.size()) {
    finish_check_failure_();
    return;
  }

  if (!start_dns_query_())
    try_next_dns_server_();
}

void WatchdogComponent::finish_check_failure_() {
  ESP_LOGW(TAG, "Internet unreachable through all configured DNS servers");
  publish_status_("Internet unreachable");
  publish_failure_("DNS servers unreachable");
  publish_internet_ok_(false);
  reset_dns_query_();
  handle_auto_restart_request_();
}

void WatchdogComponent::handle_auto_restart_request_() {
  consecutive_failure_rounds_++;
  const bool maintenance = maintenance_switch_ != nullptr && maintenance_switch_->state;
  if (maintenance) {
    ESP_LOGI(TAG, "Maintenance mode active, skipping power cycle");
    last_check_time_ = millis();
    return;
  }

  if (consecutive_failure_rounds_ < failure_threshold_) {
    ESP_LOGW(TAG, "Failure round %u/%u, waiting before restart", consecutive_failure_rounds_, failure_threshold_);
    last_check_time_ = millis();
    return;
  }

  consecutive_failure_rounds_ = 0;
  restart_attempts_++;
  if (restart_count_sensor_ != nullptr)
    restart_count_sensor_->publish_state(restart_attempts_);
  power_cycle_();
}

void WatchdogComponent::handle_network_unavailable_() {
  ESP_LOGW(TAG, "Network connection timeout");
  publish_status_("Network unavailable");
  publish_failure_("Network unavailable");
  publish_internet_ok_(false);
  reset_dns_query_();

  const bool maintenance = maintenance_switch_ != nullptr && maintenance_switch_->state;
  if (!maintenance && device_reboot_before_power_cycle_ && !device_reboot_attempted_) {
    ESP_LOGW(TAG, "Restarting device once before power cycling relay");
    device_reboot_attempted_ = true;
    device_reboot_pref_.save(&device_reboot_attempted_);
    global_preferences->sync();
    App.safe_reboot();
    return;
  }

  handle_auto_restart_request_();
  next_network_failure_check_ = millis() + check_interval_;
}

void WatchdogComponent::update_maintenance_timeout_() {
  const bool active = maintenance_switch_ != nullptr && maintenance_switch_->state;
  if (active && !maintenance_was_active_) {
    maintenance_started_at_ = millis();
    maintenance_was_active_ = true;
    ESP_LOGI(TAG, "Maintenance mode enabled");
  } else if (!active) {
    maintenance_was_active_ = false;
  }

  if (active && maintenance_timeout_ > 0 && millis() - maintenance_started_at_ >= maintenance_timeout_) {
    ESP_LOGI(TAG, "Maintenance mode timeout expired, resuming normal operation");
    maintenance_switch_->turn_off();
    maintenance_was_active_ = false;
  }
}

void WatchdogComponent::setup() {
  ESP_LOGI(TAG, "Starting platform-independent DNS watchdog");
  next_network_failure_check_ = millis() + network_connect_timeout_;
  device_reboot_pref_ = global_preferences->make_preference<bool>(0x9C17D4A1);
  device_reboot_pref_.load(&device_reboot_attempted_);
  publish_status_("Starting");

  if (restart_count_sensor_ != nullptr)
    restart_count_sensor_->publish_state(0);
  if (relay_ != nullptr)
    relay_->turn_on();

  open_dns_socket_();
}

void WatchdogComponent::loop() {
  update_maintenance_timeout_();

  if (restart_state_ == RestartState::BACKOFF_WAIT) {
    if (static_cast<int32_t>(millis() - next_restart_allowed_) < 0)
      return;
    ESP_LOGI(TAG, "Backoff expired, resuming monitoring");
    restart_state_ = RestartState::IDLE;
    last_check_time_ = millis() - check_interval_;
    return;
  }

  if (restart_state_ == RestartState::POWER_OFF_WAIT) {
    if (millis() - restart_timer_ >= power_off_time_) {
      ESP_LOGI(TAG, "Power restored");
      if (relay_ != nullptr)
        relay_->turn_on();
      restart_timer_ = millis();
      next_network_failure_check_ = restart_timer_ + network_connect_timeout_;
      restart_state_ = RestartState::BOOT_WAIT;
    }
    return;
  }

  if (restart_state_ == RestartState::BOOT_WAIT) {
    if (millis() - restart_timer_ < boot_wait_time_)
      return;

    ESP_LOGI(TAG, "Restart sequence complete");
    const uint32_t delay = calculate_backoff_();
    if (delay > 0) {
      next_restart_allowed_ = millis() + delay;
      restart_state_ = RestartState::BACKOFF_WAIT;
      publish_status_("Backoff");
      ESP_LOGI(TAG, "Entering backoff for %.1f s", delay / 1000.0f);
    } else {
      restart_state_ = RestartState::IDLE;
      publish_status_("Monitoring");
    }
    last_check_time_ = millis() - check_interval_;
    return;
  }

  if (!network::is_connected()) {
    reset_dns_query_();
    if (static_cast<int32_t>(millis() - next_network_failure_check_) >= 0)
      handle_network_unavailable_();
    return;
  }

  next_network_failure_check_ = millis() + network_connect_timeout_;
  if (device_reboot_attempted_) {
    device_reboot_attempted_ = false;
    device_reboot_pref_.save(&device_reboot_attempted_);
    global_preferences->sync();
  }

  if (!startup_delay_started_) {
    startup_delay_started_ = true;
    startup_delay_started_at_ = millis();
    ESP_LOGI(TAG, "Network connected, starting grace period");
    return;
  }
  if (!startup_completed_) {
    if (millis() - startup_delay_started_at_ < startup_grace_time_)
      return;
    startup_completed_ = true;
    last_check_time_ = millis() - check_interval_;
    publish_status_("Monitoring");
    ESP_LOGI(TAG, "Grace period finished");
  }

  if (dns_state_ == DnsState::WAITING) {
    if (process_dns_response_()) {
      finish_check_success_(millis() - dns_query_started_at_);
    } else if (millis() - dns_query_started_at_ >= dns_timeout_) {
      ESP_LOGW(TAG, "DNS server %s timed out", dns_servers_[current_dns_server_].c_str());
      try_next_dns_server_();
    }
    return;
  }

  if (millis() - last_check_time_ < check_interval_)
    return;

  current_dns_server_ = 0;
  if (!start_dns_query_())
    try_next_dns_server_();
}

uint32_t WatchdogComponent::calculate_backoff_() const {
  if (!reboot_backoff_ || restart_attempts_ <= 1)
    return 0;

  uint32_t delay = backoff_initial_time_;
  for (uint32_t attempt = 2; attempt < restart_attempts_; attempt++) {
    delay = std::min(static_cast<uint32_t>(delay * backoff_multiplier_), backoff_max_time_);
  }
  return delay;
}

void WatchdogComponent::power_cycle_() {
  if (relay_ == nullptr || restart_state_ != RestartState::IDLE)
    return;
  publish_status_("Restarting");
  relay_->turn_off();
  restart_timer_ = millis();
  restart_state_ = RestartState::POWER_OFF_WAIT;
}

}  // namespace internet_watchdog
}  // namespace esphome
