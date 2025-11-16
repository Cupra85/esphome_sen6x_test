from esphome.components import automation
from esphome.const import CONF_ID
import esphome.codegen as cg

sen6x_ns = cg.esphome_ns.namespace("sen6x")
SEN5XComponent = sen6x_ns.class_("SEN5XComponent", cg.PollingComponent, cg.I2CDevice)

StartFanAction = sen6x_ns.class_("StartFanAction", automation.Action)
StopMeasurementAction = sen6x_ns.class_("StopMeasurementAction", automation.Action)
StartMeasurementAction = sen6x_ns.class_("StartMeasurementAction", automation.Action)

automation.register_action(
    "sen6x.start_fan_cleaning", StartFanAction, automation.maybe_simple_id(SEN5XComponent)
)
automation.register_action(
    "sen6x.stop_measurement", StopMeasurementAction, automation.maybe_simple_id(SEN5XComponent)
)
automation.register_action(
    "sen6x.start_measurement", StartMeasurementAction, automation.maybe_simple_id(SEN5XComponent)
)
