#include "watchdog.h"
#include "esphome/core/log.h"
#include <ESP8266WiFi.h>

namespace esphome {
namespace watchdog {

static const char *const TAG = "watchdog";
void WatchdogComponent::publish_status(const char *status) {
    if (status_sensor_)
        status_sensor_->publish_state(status);
}

void WatchdogComponent::publish_target(const char *target) {
    if (target_sensor_)
        target_sensor_->publish_state(target);
}

void WatchdogComponent::publish_latency(uint32_t latency) {
    if (latency_sensor_)
        latency_sensor_->publish_state(latency);
}

void WatchdogComponent::publish_failure(const char *failure) {
    if (failure_sensor_)
        failure_sensor_->publish_state(failure);
}

void WatchdogComponent::publish_internet_ok(bool ok) {
    if (internet_ok_sensor_)
        internet_ok_sensor_->publish_state(ok);
}

void WatchdogComponent::set_gateway(const std::string &gateway) {
    gateway_ = gateway;
}

void WatchdogComponent::set_hosts(const std::vector<std::string> &hosts) {
    hosts_ = hosts;
}

void WatchdogComponent::set_startup_grace_time(uint32_t ms) {
    startup_grace_time_ = ms;
}

void WatchdogComponent::set_power_off_time(uint32_t ms) {
    power_off_time_ = ms;
}

void WatchdogComponent::set_boot_wait_time(uint32_t ms) {
    boot_wait_time_ = ms;
}
void WatchdogComponent::set_backoff_initial_time(uint32_t ms) {
    backoff_initial_time_ = ms;
}

void WatchdogComponent::set_backoff_max_time(uint32_t ms) {
    backoff_max_time_ = ms;
}

void WatchdogComponent::set_backoff_multiplier(float multiplier) {
    backoff_multiplier_ = multiplier;
}

void WatchdogComponent::setup() {
    ESP_LOGI(TAG, "Watchdog started");

    publish_status("Starting");
    if (relay_ != nullptr) {
        ESP_LOGI("watchdog", "Relay configured");
    } else {
        ESP_LOGI("watchdog", "No relay configured");
    }
    if (relay_ != nullptr) {
        relay_->turn_off();
        delay(1000);
        relay_->turn_on();
    }

    // Callback för varje svar eller timeout
    ping_.on(true, [this](const AsyncPingResponse &response) {
        if (response.answer) {
            ping_success_ = true;
            ping_latency_ = response.time;

            publish_latency(response.time);

            ESP_LOGI(TAG, "Reply: %u ms", response.time);
        } else {
            ESP_LOGW(TAG, "Timeout");
        }

        return false;
    });

    // Callback när pingningen är färdig
    ping_.on(false, [this](const AsyncPingResponse &response) {
        ping_finished_ = true;
        ping_running_ = false;

        ESP_LOGI(TAG,
                 "Finished: sent=%u recv=%u",
                 response.total_sent,
                 response.total_recv);

        return false;
    });
}

void WatchdogComponent::start_ping(const char *host) {
    ping_finished_ = false;
    ping_success_ = false;
    ping_latency_ = 0;
    ping_running_ = true;

    if (ping_stage_ == PingStage::GATEWAY)
        publish_target("Gateway");
    else
        publish_target(host);

    if (ping_stage_ == PingStage::GATEWAY) {
        if (status_sensor_)
            status_sensor_->publish_state("Checking gateway");

        ESP_LOGI(TAG, "Pinging gateway %s...", host);
    } else {
        if (status_sensor_)
            status_sensor_->publish_state("Checking internet");

        ESP_LOGI(TAG, "Pinging host #%u (%s)...",
                 (unsigned) current_host_, host);
    }

    ping_.begin(host, 1, 1000);
}

void WatchdogComponent::loop() {
    static uint32_t last = 0;
    if (restart_state_ == RestartState::BACKOFF_WAIT) {

        if (millis() < next_restart_allowed_)
            return;

        ESP_LOGI(TAG, "Backoff expired - restarting modem");

        power_cycle();

        return;
    }
    if (restart_state_ == RestartState::POWER_OFF_WAIT) {
    if (millis() - restart_timer_ >= power_off_time_) {
        ESP_LOGI(TAG, "Power restored");

        relay_->turn_on();

        restart_timer_ = millis();
        restart_state_ = RestartState::BOOT_WAIT;
    }

    return;
    }

    if (restart_state_ == RestartState::BOOT_WAIT) {
        if (millis() - restart_timer_ >= boot_wait_time_) {
            ESP_LOGI(TAG, "Restart sequence complete");

            publish_status("Monitoring");

            ping_stage_ = PingStage::GATEWAY;
            current_host_ = 0;
            restart_attempts_++;
            
            uint32_t delay = calculate_backoff();

            if (delay > 0) {

                next_restart_allowed_ = millis() + delay;

                restart_state_ = RestartState::BACKOFF_WAIT;

                publish_status("Backoff");

                ESP_LOGI(TAG, "Entering backoff for %u ms", delay);

            } else {

                restart_state_ = RestartState::IDLE;
            }

            last = millis();
        }

        return;
    }

    // Vänta tills WiFi är anslutet
    if (!startup_delay_started_) {
        if (!WiFi.isConnected())
            return;

        startup_delay_started_ = true;
        startup_delay_ms_ = millis();

        ESP_LOGI(TAG, "WiFi connected, starting 30 second grace period");
        return;
    }

    // Grace period
    if (!startup_completed_) {
        if (millis() - startup_delay_ms_ < startup_grace_time_)
            return;

        startup_completed_ = true;

        publish_status("Monitoring");

        ESP_LOGI(TAG, "Grace period finished");
    }

    // Starta ny ping
    if (!ping_running_ && millis() - last >= 5000) {
        last = millis();

        if (ping_stage_ == PingStage::GATEWAY) {
            start_ping(gateway_.c_str());
        } else {
            start_ping(hosts_[current_host_].c_str());
        }
    }

    // Vänta tills pingningen är klar
    if (!ping_finished_)
        return;

    ping_finished_ = false;

    if (ping_stage_ == PingStage::GATEWAY) {

        if (ping_success_) {
            publish_status("Gateway OK");

            ESP_LOGI(TAG, "Gateway OK");

            ping_stage_ = PingStage::INTERNET;
            current_host_ = 0;
        } else {
            publish_status("Gateway unreachable");
            publish_failure("Gateway unreachable");
            publish_internet_ok(false);

            ESP_LOGW(TAG, "Gateway unreachable");
            // Här kommer senare power_cycle();
            power_cycle();
        }

        return;
    }

    // Internettest
    if (ping_success_) {

        publish_status("OK");
        publish_internet_ok(true);
        publish_failure("None");
        restart_attempts_ = 0;
        next_restart_allowed_ = 0;


        ESP_LOGI(TAG,
                 "Internet OK via %s (%u ms)",
                 hosts_[current_host_].c_str(),
                 ping_latency_);

        ping_stage_ = PingStage::GATEWAY;
        current_host_ = 0;

    } else {

        current_host_++;

        if (current_host_ >= hosts_.size()) {

            publish_status("Internet unreachable");
            publish_failure("Internet unreachable");
            publish_internet_ok(false);

            ESP_LOGW(TAG, "Internet unreachable");

            ping_stage_ = PingStage::GATEWAY;
            current_host_ = 0;

            // Här kommer senare power_cycle();
            power_cycle();
        }
    }
}
uint32_t WatchdogComponent::calculate_backoff() const {

    if (restart_attempts_ <= 1)
        return 0;

    uint32_t delay = backoff_initial_time_;

    for (uint32_t i = 2; i < restart_attempts_; i++) {
        delay = std::min(
            (uint32_t)(delay * backoff_multiplier_),
            backoff_max_time_);
    }

    return delay;
}
void WatchdogComponent::power_cycle() {
    if (relay_ == nullptr)
        return;

    if (restart_state_ != RestartState::IDLE &&
        restart_state_ != RestartState::BACKOFF_WAIT)
        return;

    
    publish_status("Restarting");

    relay_->turn_off();

    restart_timer_ = millis();
    restart_state_ = RestartState::POWER_OFF_WAIT;
}


}  // namespace watchdog
}  // namespace esphome