import esphome.codegen as cg
from esphome.components import automation

# Namespace & Klassen müssen exakt zu deinem C++ passen:
sen6x_ns = cg.esphome_ns.namespace("sen6x")
SEN5XComponent = sen6x_ns.class_("SEN5XComponent", cg.PollingComponent, cg.I2CDevice)

# Action-Klassen referenzieren
StartFanAction = sen6x_ns.class_("StartFanAction", automation.Action)
StopMeasurementAction = sen6x_ns.class_("StopMeasurementAction", automation.Action)
StartMeasurementAction = sen6x_ns.class_("StartMeasurementAction", automation.Action)

# Header EINBINDEN, damit der Codegenerator die Klassen kennt:
cg.add(cg.include("sen6x/automation.h"))

# Actions registrieren – die Namen müssen exakt so ins YAML:
automation.register_action(
    "sen6x.start_fan_cleaning", StartFanAction, automation.maybe_simple_id(SEN5XComponent)
)
automation.register_action(
    "sen6x.stop_measurement", StopMeasurementAction, automation.maybe_simple_id(SEN5XComponent)
)
automation.register_action(
    "sen6x.start_measurement", StartMeasurementAction, automation.maybe_simple_id(SEN5XComponent)
)

