import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL

AUTO_LOAD = ["i2c"]
sen6x_ns = cg.namespace("esphome").namespace("sen6x")
SEN6xComponent = sen6x_ns.class_("SEN6xComponent", cg.PollingComponent, i2c.I2CDevice)

Variant = sen6x_ns.enum("Variant")
VARIANT = cv.enum({
    "AUTO": Variant.AUTO,
    "SEN60": Variant.SEN60,
    "SEN63C": Variant.SEN63C,
    "SEN65": Variant.SEN65,
    "SEN66": Variant.SEN66,
    "SEN68": Variant.SEN68,
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SEN6xComponent),
    cv.Optional("variant", default="AUTO"): VARIANT,
    cv.Optional("use_data_ready", default=True): cv.boolean,
    cv.Optional(CONF_UPDATE_INTERVAL, default="1s"): cv.positive_time_period_milliseconds,
}).extend(i2c.i2c_device_schema(0x6B))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_variant(config.get("variant")))
    cg.add(var.set_update_every(config.get("use_data_ready")))
