#pragma once

#include "esphome/core/component.h"
#include <AsyncPing.h>
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

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

private:
    enum class PingStage {
        GATEWAY,
        INTERNET
    };

    static constexpr const char *GATEWAY_HOST = "192.168.8.1";

    //static constexpr const char *INTERNET_HOSTS[] = {
    //    "1.1.1.1",
    //    "8.8.8.8",
    //    "9.9.9.9"
    //};
    static constexpr const char *INTERNET_HOSTS[] = {
        "10.255.255.1",
        "10.255.255.2",
        "10.255.255.3"
    };

    static constexpr size_t INTERNET_HOST_COUNT =
        sizeof(INTERNET_HOSTS) / sizeof(INTERNET_HOSTS[0]);

    void start_ping(const char *host);

    void publish_status(const char *status);
    void publish_target(const char *target);
    void publish_latency(uint32_t latency);
    void publish_failure(const char *failure);
    void publish_internet_ok(bool ok);

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

    sensor::Sensor *latency_sensor_{nullptr};
    sensor::Sensor *restart_count_sensor_{nullptr};

    text_sensor::TextSensor *status_sensor_{nullptr};
    text_sensor::TextSensor *target_sensor_{nullptr};
    text_sensor::TextSensor *failure_sensor_{nullptr};

    binary_sensor::BinarySensor *internet_ok_sensor_{nullptr};
};

}  // namespace watchdog
}  // namespace esphome