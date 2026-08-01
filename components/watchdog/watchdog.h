#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include <AsyncPing.h>
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace watchdog {

class WatchdogComponent : public Component {
public:
    void setup() override;
    void loop() override;
    

    void set_latency_sensor(sensor::Sensor *sensor) { latency_sensor_ = sensor; }
    void set_restart_count_sensor(sensor::Sensor *sensor) { restart_count_sensor_ = sensor; }

    void set_status_sensor(text_sensor::TextSensor *sensor) { status_sensor_ = sensor; }
    void set_target_sensor(text_sensor::TextSensor *sensor) { target_sensor_ = sensor; }
    void set_failure_sensor(text_sensor::TextSensor *sensor) { failure_sensor_ = sensor; }

    void set_internet_ok_sensor(binary_sensor::BinarySensor *sensor) { internet_ok_sensor_ = sensor; }
    void set_relay(switch_::Switch *relay) { relay_ = relay; }
    void set_gateway(const std::string &gateway);
    void set_hosts(const std::vector<std::string> &hosts);

    void set_startup_grace_time(uint32_t ms);
    void set_power_off_time(uint32_t ms);
    void set_boot_wait_time(uint32_t ms);
    void set_backoff_initial_time(uint32_t ms);
    void set_backoff_max_time(uint32_t ms);
    void set_backoff_multiplier(float multiplier);
    void set_maintenance_switch(switch_::Switch *sw);
    void set_ping_interval(uint32_t ms) {ping_interval_ = ms;}
    void set_reboot_backoff(bool enabled) { reboot_backoff_ = enabled; }

private:
    enum class PingStage {
        GATEWAY,
        INTERNET
    };
    enum class RestartState {
        IDLE,
        BACKOFF_WAIT,
        POWER_OFF_WAIT,
        BOOT_WAIT
    };

    //static constexpr const char *GATEWAY_HOST = "192.168.8.1";

    //static constexpr const char *INTERNET_HOSTS[] = {
    //    "1.1.1.1",
    //    "8.8.8.8",
    //    "9.9.9.9"
    //};
    //static constexpr const char *INTERNET_HOSTS[] = {
    //    "10.255.255.1",
    //    "10.255.255.2",
    //    "10.255.255.3"
    //};

    //static constexpr size_t INTERNET_HOST_COUNT =
    //    sizeof(INTERNET_HOSTS) / sizeof(INTERNET_HOSTS[0]);

    void start_ping(const char *host);

    void publish_status(const char *status);
    void publish_target(const char *target);
    void publish_latency(uint32_t latency);
    void publish_failure(const char *failure);
    void publish_internet_ok(bool ok);
    void power_cycle();
    void handle_auto_restart_request();

    AsyncPing ping_;

    PingStage ping_stage_{PingStage::GATEWAY};

    size_t current_host_{0};

    bool startup_delay_started_{false};
    uint32_t startup_delay_ms_{0};
    bool startup_completed_{false};

    bool ping_running_{false};
    bool ping_finished_{false};
    bool ping_success_{false};

    uint32_t ping_latency_{0};
    std::string gateway_;
    std::vector<std::string> hosts_;

    uint32_t startup_grace_time_{30000};
    uint32_t power_off_time_{20000};
    uint32_t boot_wait_time_{180000};
    uint32_t backoff_initial_time_{300000};     // 5 min
    uint32_t backoff_max_time_{3600000};        // 60 min
    float backoff_multiplier_{2.0f};
    bool reboot_backoff_{true};

    uint32_t restart_attempts_{0};
    uint32_t next_restart_allowed_{0};
    uint32_t ping_interval_{60000};
    bool ping_round_active_{false};
    uint32_t last_ping_time_{0};


    

    sensor::Sensor *latency_sensor_{nullptr};
    sensor::Sensor *restart_count_sensor_{nullptr};

    text_sensor::TextSensor *status_sensor_{nullptr};
    text_sensor::TextSensor *target_sensor_{nullptr};
    text_sensor::TextSensor *failure_sensor_{nullptr};

    binary_sensor::BinarySensor *internet_ok_sensor_{nullptr};
    switch_::Switch *relay_{nullptr};
    RestartState restart_state_{RestartState::IDLE};
    uint32_t restart_timer_{0};
    uint32_t calculate_backoff() const;

    switch_::Switch *maintenance_switch_{nullptr};

   
};

}  // namespace watchdog
}  // namespace esphome
