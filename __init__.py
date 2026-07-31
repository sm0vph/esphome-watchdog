import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import sensor, text_sensor, binary_sensor, switch
from esphome.const import CONF_ID, CONF_NAME

AUTO_LOAD = [
    "sensor",
    "text_sensor",
    "binary_sensor",
    "switch",
]

watchdog_ns = cg.esphome_ns.namespace("watchdog")

WatchdogComponent = watchdog_ns.class_(
    "WatchdogComponent",
    cg.Component,
)

CONF_STATUS = "status"
CONF_LATENCY = "latency"
CONF_CURRENT_TARGET = "current_target"
CONF_LAST_FAILURE = "last_failure"
CONF_INTERNET_OK = "internet_ok"
CONF_RESTART_COUNT = "restart_count"
CONF_RELAY = "relay"
CONF_POWER_OFF_TIME = "power_off_time"
CONF_BOOT_WAIT_TIME = "boot_wait_time"
CONF_STARTUP_GRACE_TIME = "startup_grace_time"
CONF_GATEWAY = "gateway"
CONF_HOSTS = "hosts"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WatchdogComponent),

        cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_CURRENT_TARGET): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_LAST_FAILURE): text_sensor.text_sensor_schema(),

        cv.Optional(CONF_LATENCY): sensor.sensor_schema(
            unit_of_measurement="ms",
            accuracy_decimals=0,
        ),

        cv.Optional(CONF_RESTART_COUNT): sensor.sensor_schema(
            accuracy_decimals=0,
        ),

        cv.Optional(CONF_INTERNET_OK): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_RELAY): cv.use_id(switch.Switch),
        cv.Optional(CONF_POWER_OFF_TIME, default="20s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_BOOT_WAIT_TIME, default="180s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_STARTUP_GRACE_TIME, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_GATEWAY, default="192.168.1.1"): cv.string,

        cv.Optional(CONF_HOSTS,default=["1.1.1.1", "8.8.8.8", "9.9.9.9"],): cv.All(cv.ensure_list(cv.string),cv.Length(min=1),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STATUS])
        cg.add(var.set_status_sensor(sens))

    if CONF_CURRENT_TARGET in config:
        sens = await text_sensor.new_text_sensor(config[CONF_CURRENT_TARGET])
        cg.add(var.set_target_sensor(sens))

    if CONF_LAST_FAILURE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_FAILURE])
        cg.add(var.set_failure_sensor(sens))

    if CONF_LATENCY in config:
        sens = await sensor.new_sensor(config[CONF_LATENCY])
        cg.add(var.set_latency_sensor(sens))

    if CONF_RESTART_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_RESTART_COUNT])
        cg.add(var.set_restart_count_sensor(sens))

    if CONF_INTERNET_OK in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_INTERNET_OK])
        cg.add(var.set_internet_ok_sensor(sens))
    if CONF_RELAY in config:
        relay = await cg.get_variable(config[CONF_RELAY])
        cg.add(var.set_relay(relay))
    cg.add(var.set_power_off_time(config[CONF_POWER_OFF_TIME]))
    cg.add(var.set_boot_wait_time(config[CONF_BOOT_WAIT_TIME]))
    cg.add(var.set_startup_grace_time(config[CONF_STARTUP_GRACE_TIME]))
    cg.add(var.set_gateway(config[CONF_GATEWAY]))
    cg.add(var.set_hosts(config[CONF_HOSTS]))
