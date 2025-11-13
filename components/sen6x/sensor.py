from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor, text_sensor, button
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_OFFSET,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_CO2,
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE,
    CONF_TEMPERATURE_COMPENSATION,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_TEMPERATURE,
    ICON_CHEMICAL_WEAPON,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    ICON_MOLECULE_CO2,
    ICON_FAN,
    ICON_POWER,
    ICON_RESTART,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

CODEOWNERS = ["@martgras"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common", "text_sensor", "button"]

sen6x_ns = cg.esphome_ns.namespace("sen6x")
SEN5XComponent = sen6x_ns.class_("SEN5XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice)

# Actions
StartFanAction = sen6x_ns.class_("StartFanAction", automation.Action)
ResetAction = sen6x_ns.class_("ResetAction", automation.Action)
ReadStatusAction = sen6x_ns.class_("ReadStatusAction", automation.Action)
StartMeasAction = sen6x_ns.class_("StartMeasAction", automation.Action)
StopMeasAction = sen6x_ns.class_("StopMeasAction", automation.Action)
HeaterOnAction = sen6x_ns.class_("HeaterOnAction", automation.Action)

CONF_ALGORITHM_TUNING = "algorithm_tuning"
CONF_GAIN_FACTOR = "gain_factor"
CONF_GATING_MAX_DURATION_MINUTES = "gating_max_duration_minutes"
CONF_INDEX_OFFSET = "index_offset"
CONF_LEARNING_TIME_GAIN_HOURS = "learning_time_gain_hours"
CONF_LEARNING_TIME_OFFSET_HOURS = "learning_time_offset_hours"
CONF_NORMALIZED_OFFSET_SLOPE = "normalized_offset_slope"
CONF_NOX = "nox"
CONF_STD_INITIAL = "std_initial"
CONF_TIME_CONSTANT = "time_constant"
CONF_VOC = "voc"

# Text sensors (READ meta)
CONF_STATUS_TEXT = "status"
CONF_VERSION_TEXT = "version"
CONF_SERIAL_TEXT = "serial"
CONF_PRODUCT_TEXT = "product_name"

# Number concentration sensors
CONF_NC_03 = "nc_0_3"
CONF_NC_05 = "nc_0_5"
CONF_NC_10 = "nc_1_0"
CONF_NC_25 = "nc_2_5"
CONF_NC_40 = "nc_4_0"
CONF_NC_100 = "nc_10_0"

# Raw value sensors (SEN66 4.8.14)
CONF_RAW_RH_X100 = "raw_rh_x100"
CONF_RAW_T_X200 = "raw_t_x200"
CONF_RAW_SRAW_VOC = "raw_sraw_voc"
CONF_RAW_SRAW_NOX = "raw_sraw_nox"
CONF_RAW_CO2 = "raw_co2"

def float_previously_pct(value):
    if isinstance(value, str) and "%" in value:
        raise cv.Invalid(f"The value '{value}' is a percentage. Suggested value: {float(value.strip('%')) / 100}")
    return value

GAS_SENSOR = cv.Schema(
    {
        cv.Optional(CONF_ALGORITHM_TUNING): cv.Schema(
            {
                cv.Optional(CONF_INDEX_OFFSET, default=100): cv.int_range(1, 250),
                cv.Optional(CONF_LEARNING_TIME_OFFSET_HOURS, default=12): cv.int_range(1, 1000),
                cv.Optional(CONF_LEARNING_TIME_GAIN_HOURS, default=12): cv.int_range(1, 1000),
                cv.Optional(CONF_GATING_MAX_DURATION_MINUTES, default=720): cv.int_range(0, 3000),
                cv.Optional(CONF_STD_INITIAL, default=50): cv.int_,
                cv.Optional(CONF_GAIN_FACTOR, default=230): cv.int_range(1, 1000),
            }
        )
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SEN5XComponent),

            # Standard outputs
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOC): sensor.sensor_schema(
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GAS_SENSOR),
            cv.Optional(CONF_NOX): sensor.sensor_schema(
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GAS_SENSOR),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            # Number concentration (x10 scaling -> wir geben als float aus)
            cv.Optional(CONF_NC_03): sensor.sensor_schema(icon=ICON_CHEMICAL_WEAPON, accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_NC_05): sensor.sensor_schema(icon=ICON_CHEMICAL_WEAPON, accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_NC_10): sensor.sensor_schema(icon=ICON_CHEMICAL_WEAPON, accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_NC_25): sensor.sensor_schema(icon=ICON_CHEMICAL_WEAPON, accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_NC_40): sensor.sensor_schema(icon=ICON_CHEMICAL_WEAPON, accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_NC_100): sensor.sensor_schema(icon=ICON_CHEMICAL_WEAPON, accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT),

            # Raw values
            cv.Optional(CONF_RAW_RH_X100): sensor.sensor_schema(icon=ICON_WATER_PERCENT, accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_RAW_T_X200): sensor.sensor_schema(icon=ICON_THERMOMETER, accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_RAW_SRAW_VOC): sensor.sensor_schema(icon=ICON_RADIATOR, accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_RAW_SRAW_NOX): sensor.sensor_schema(icon=ICON_RADIATOR, accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT),
            cv.Optional(CONF_RAW_CO2): sensor.sensor_schema(icon=ICON_MOLECULE_CO2, accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT),

            # TextSensoren
            cv.Optional(CONF_STATUS_TEXT): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_VERSION_TEXT): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_SERIAL_TEXT): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_PRODUCT_TEXT): text_sensor.text_sensor_schema(),

            # Buttons (SEND)
            

            cv.Optional("start_measurement"): button.button_schema(StartMeasAction, icon=ICON_POWER),
            cv.Optional("stop_measurement"): button.button_schema(StopMeasAction, icon=ICON_POWER),
            cv.Optional("fan_cleaning"): button.button_schema(StartFanAction, icon=ICON_FAN),
            cv.Optional("heater_on"): button.button_schema(HeaterOnAction, icon=ICON_POWER),
            cv.Optional("reset"): button.button_schema(ResetAction, icon=ICON_RESTART),


            cv.Optional(CONF_STORE_BASELINE, default=True): cv.boolean,
            cv.Optional(CONF_TEMPERATURE_COMPENSATION): cv.Schema({
                cv.Optional(CONF_OFFSET, default=0): cv.float_,
                cv.Optional(CONF_NORMALIZED_OFFSET_SLOPE, default=0): cv.All(float_previously_pct, cv.float_),
                cv.Optional(CONF_TIME_CONSTANT, default=0): cv.int_,
            })
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B))
)

SENSOR_MAP = {
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_4_0: "set_pm_4_0_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    CONF_VOC: "set_voc_sensor",
    CONF_NOX: "set_nox_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
    CONF_CO2: "set_co2_sensor",

    # NC
    CONF_NC_03: "set_nc_03_sensor",
    CONF_NC_05: "set_nc_05_sensor",
    CONF_NC_10: "set_nc_10_sensor",
    CONF_NC_25: "set_nc_25_sensor",
    CONF_NC_40: "set_nc_40_sensor",
    CONF_NC_100: "set_nc_100_sensor",

    # Raw
    CONF_RAW_RH_X100: "set_raw_rh_x100_sensor",
    CONF_RAW_T_X200: "set_raw_t_x200_sensor",
    CONF_RAW_SRAW_VOC: "set_raw_sraw_voc_sensor",
    CONF_RAW_SRAW_NOX: "set_raw_sraw_nox_sensor",
    CONF_RAW_CO2: "set_raw_co2_sensor",
}

TEXT_SENSOR_MAP = {
    CONF_STATUS_TEXT: "set_status_text_sensor",
    CONF_VERSION_TEXT: "set_version_text_sensor",
    CONF_SERIAL_TEXT: "set_serial_text_sensor",
    CONF_PRODUCT_TEXT: "set_product_text_sensor",
}

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Standard + NC + Raw numeric sensors
    for key, func in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, func)(sens))

    # Text sensors
    for key, func in TEXT_SENSOR_MAP.items():
        if key in config:
            ts = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, func)(ts))

    # Buttons
    if "start_measurement" in config:
        b = await button.new_button(config["start_measurement"])
        b.set_pressed_action(lambda: cg.add(var.start_continuous_measurement()))
    if "stop_measurement" in config:
        b = await button.new_button(config["stop_measurement"])
        b.set_pressed_action(lambda: cg.add(var.stop_measurement()))
    if "fan_cleaning" in config:
        b = await button.new_button(config["fan_cleaning"])
        b.set_pressed_action(lambda: cg.add(var.start_fan_cleaning()))
    if "heater_on" in config:
        b = await button.new_button(config["heater_on"])
        b.set_pressed_action(lambda: cg.add(var.activate_sht_heater()))
    if "reset" in config:
        b = await button.new_button(config["reset"])
        b.set_pressed_action(lambda: cg.add(var.device_reset()))

    # Settings
    if CONF_STORE_BASELINE in config:
        cg.add(var.set_store_baseline(config[CONF_STORE_BASELINE]))

    if CONF_TEMPERATURE_COMPENSATION in config:
        cfg = config[CONF_TEMPERATURE_COMPENSATION]
        cg.add(var.set_temperature_compensation(cfg[CONF_OFFSET], cfg[CONF_NORMALIZED_OFFSET_SLOPE], cfg[CONF_TIME_CONSTANT]))

    # VOC/NOx tuning optional
    if CONF_VOC in config and CONF_ALGORITHM_TUNING in config[CONF_VOC]:
        cfg = config[CONF_VOC][CONF_ALGORITHM_TUNING]
        cg.add(var.set_voc_algorithm_tuning(
            cfg[CONF_INDEX_OFFSET],
            cfg[CONF_LEARNING_TIME_OFFSET_HOURS],
            cfg[CONF_LEARNING_TIME_GAIN_HOURS],
            cfg[CONF_GATING_MAX_DURATION_MINUTES],
            cfg[CONF_STD_INITIAL],
            cfg[CONF_GAIN_FACTOR]
        ))

    if CONF_NOX in config and CONF_ALGORITHM_TUNING in config[CONF_NOX]:
        cfg = config[CONF_NOX][CONF_ALGORITHM_TUNING]
        cg.add(var.set_nox_algorithm_tuning(
            cfg[CONF_INDEX_OFFSET],
            cfg[CONF_LEARNING_TIME_OFFSET_HOURS],
            cfg[CONF_LEARNING_TIME_GAIN_HOURS],
            cfg[CONF_GATING_MAX_DURATION_MINUTES],
            cfg[CONF_GAIN_FACTOR]
        ))

# Optional: register actions if du sie in YAML als automations benutzen willst
@automation.register_action("sen6x.reset", ResetAction, maybe_simple_id({cv.GenerateID(): cv.use_id(SEN5XComponent)}))
async def sen6x_reset_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, var)

@automation.register_action("sen6x.read_status", ReadStatusAction, maybe_simple_id({cv.GenerateID(): cv.use_id(SEN5XComponent)}))
async def sen6x_read_status_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, var)

@automation.register_action("sen6x.start_measurement", StartMeasAction, maybe_simple_id({cv.GenerateID(): cv.use_id(SEN5XComponent)}))
async def sen6x_start_meas_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, var)

@automation.register_action("sen6x.stop_measurement", StopMeasAction, maybe_simple_id({cv.GenerateID(): cv.use_id(SEN5XComponent)}))
async def sen6x_stop_meas_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, var)

@automation.register_action("sen6x.heater_on", HeaterOnAction, maybe_simple_id({cv.GenerateID(): cv.use_id(SEN5XComponent)}))
async def sen6x_heater_on_to_code(config, action_id, template_arg, args):
    var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, var)
