# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Brian Towles

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.const import CONF_ID, CONF_NAME, CONF_TRIGGER_ID

CODEOWNERS = ["@wonderslug"]
DEPENDENCIES = ["esp32"]

ancs_ns = cg.esphome_ns.namespace("ancs")
AncsComponent = ancs_ns.class_("AncsComponent", cg.Component)

AttributeId = ancs_ns.namespace("protocol").enum("AttributeId", is_class=True)
_FETCH_MAP = {
    "app_id": AttributeId.APP_IDENTIFIER,
    "title": AttributeId.TITLE,
    "subtitle": AttributeId.SUBTITLE,
    "message": AttributeId.MESSAGE,
}

ConnectTrigger = ancs_ns.class_("ConnectTrigger", automation.Trigger.template())
DisconnectTrigger = ancs_ns.class_("DisconnectTrigger", automation.Trigger.template())
NotificationAddedTrigger = ancs_ns.class_("NotificationAddedTrigger", automation.Trigger.template())
NotificationModifiedTrigger = ancs_ns.class_("NotificationModifiedTrigger", automation.Trigger.template())
NotificationRemovedTrigger = ancs_ns.class_("NotificationRemovedTrigger", automation.Trigger.template())
NotificationAttributesTrigger = ancs_ns.class_("NotificationAttributesTrigger", automation.Trigger.template())

ClearBondsAction = ancs_ns.class_("ClearBondsAction", automation.Action)
DisconnectAction = ancs_ns.class_("DisconnectAction", automation.Action)
RequestAttributesAction = ancs_ns.class_("RequestAttributesAction", automation.Action)

CONF_ON_CONNECT = "on_connect"
CONF_ON_DISCONNECT = "on_disconnect"
CONF_ON_NOTIFICATION_ADDED = "on_notification_added"
CONF_ON_NOTIFICATION_MODIFIED = "on_notification_modified"
CONF_ON_NOTIFICATION_REMOVED = "on_notification_removed"
CONF_ON_NOTIFICATION_ATTRIBUTES = "on_notification_attributes"

CONF_AUTO_FETCH_ATTRIBUTES = "auto_fetch_attributes"
CONF_FETCH_ATTRIBUTES = "fetch_attributes"
CONF_MAX_BONDS = "max_bonds"
CONF_MAX_CONNECTIONS = "max_connections"
CONF_MANUFACTURER = "manufacturer"
CONF_MODEL = "model"
CONF_NIMBLE_HOST_TASK_STACK_SIZE = "nimble_host_task_stack_size"

FETCH_ATTRIBUTE_OPTIONS = ["app_id", "title", "subtitle", "message"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AncsComponent),
        cv.Optional(CONF_NAME, default="ESPHome-ANCS"): cv.All(cv.string, cv.Length(max=29)),
        cv.Optional(CONF_AUTO_FETCH_ATTRIBUTES, default=True): cv.boolean,
        cv.Optional(CONF_FETCH_ATTRIBUTES, default=["app_id", "title", "message"]): cv.ensure_list(
            cv.one_of(*FETCH_ATTRIBUTE_OPTIONS, lower=True)
        ),
        cv.Optional(CONF_MANUFACTURER, default="ESPHome"): cv.All(cv.string, cv.Length(max=20)),
        cv.Optional(CONF_MODEL, default="ANCS Node"): cv.All(cv.string, cv.Length(max=20)),
        cv.Optional(CONF_MAX_BONDS, default=3): cv.int_range(min=1, max=9),
        cv.Optional(CONF_MAX_CONNECTIONS, default=3): cv.int_range(min=1, max=7),
        # Stack size (bytes) for the NimBLE host task. LE Secure Connections runs
        # P-256 ECDH key generation on this task, and all of our GAP/GATT callbacks
        # (stack buffers + ESPHome logging) run on it too. The ESP-IDF default of
        # 4096 overflows during the pairing/encryption exchange and reboots the
        # device with "stack overflow in task nimble_host" (see the connect →
        # enc_change storm in the crash logs). 8192 gives comfortable headroom;
        # raise it further only if a custom build still overflows.
        cv.Optional(CONF_NIMBLE_HOST_TASK_STACK_SIZE, default=8192): cv.int_range(min=4096, max=32768),
        cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ConnectTrigger)}
        ),
        cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DisconnectTrigger)}
        ),
        cv.Optional(CONF_ON_NOTIFICATION_ADDED): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NotificationAddedTrigger)}
        ),
        cv.Optional(CONF_ON_NOTIFICATION_MODIFIED): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NotificationModifiedTrigger)}
        ),
        cv.Optional(CONF_ON_NOTIFICATION_REMOVED): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NotificationRemovedTrigger)}
        ),
        cv.Optional(CONF_ON_NOTIFICATION_ATTRIBUTES): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NotificationAttributesTrigger)}
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_no_bluedroid_ble(config):
    # NimBLE owns the radio; reject Bluedroid-based BLE stacks in the same build.
    conflicting = {
        "esp32_ble",
        "esp32_ble_tracker",
        "esp32_ble_server",
        "bluetooth_proxy",
    }
    full_config = fv.full_config.get()
    present = conflicting.intersection(full_config.keys())
    if present:
        raise cv.Invalid(
            f"The 'ancs' component uses NimBLE and cannot be combined with "
            f"Bluedroid BLE components: {', '.join(sorted(present))}"
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_no_bluedroid_ble


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device_name(config[CONF_NAME]))
    cg.add(var.set_auto_fetch(config[CONF_AUTO_FETCH_ATTRIBUTES]))
    cg.add(var.set_max_bonds(config[CONF_MAX_BONDS]))
    cg.add(var.set_manufacturer(config[CONF_MANUFACTURER]))
    cg.add(var.set_model(config[CONF_MODEL]))
    for attr in config[CONF_FETCH_ATTRIBUTES]:
        cg.add(var.add_fetch_attribute(_FETCH_MAP[attr]))

    for conf in config.get(CONF_ON_CONNECT, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trig, [(cg.std_string, "device_name")], conf)
    for conf in config.get(CONF_ON_DISCONNECT, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trig, [(cg.std_string, "device_name")], conf)
    for conf in config.get(CONF_ON_NOTIFICATION_ADDED, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trig,
            [
                (cg.uint32, "uid"),
                (cg.std_string, "category"),
                (cg.uint8, "category_count"),
                (cg.uint8, "flags"),
                (cg.std_string, "device_name"),
            ],
            conf,
        )
    for conf in config.get(CONF_ON_NOTIFICATION_MODIFIED, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trig,
            [(cg.uint32, "uid"), (cg.std_string, "category"), (cg.uint8, "flags"), (cg.std_string, "device_name")],
            conf,
        )
    for conf in config.get(CONF_ON_NOTIFICATION_REMOVED, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trig, [(cg.uint32, "uid"), (cg.std_string, "category"), (cg.std_string, "device_name")], conf)
    for conf in config.get(CONF_ON_NOTIFICATION_ATTRIBUTES, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trig,
            [
                (cg.uint32, "uid"),
                (cg.std_string, "category"),
                (cg.std_string, "app_id"),
                (cg.std_string, "title"),
                (cg.std_string, "subtitle"),
                (cg.std_string, "message"),
                (cg.std_string, "device_name"),
            ],
            conf,
        )

    # Enable native NimBLE host via sdkconfig (ESP-IDF only).
    cg.add_platformio_option("framework", "espidf")
    for opt, val in [
        ("CONFIG_BT_ENABLED", True),
        ("CONFIG_BT_NIMBLE_ENABLED", True),
        ("CONFIG_BT_CONTROLLER_ENABLED", True),
        ("CONFIG_BT_NIMBLE_ROLE_PERIPHERAL", True),
        # Central role MUST stay enabled: ESP-IDF NimBLE compiles the GATT-client
        # procedures (ble_gattc_disc_*, ble_gattc_write_flat) only when the central
        # role is on. ANCS acts as a GATT client over the inbound peripheral link,
        # so these are required even though we never initiate connections or scan.
        ("CONFIG_BT_NIMBLE_ROLE_CENTRAL", True),
        ("CONFIG_BT_NIMBLE_ROLE_BROADCASTER", False),
        ("CONFIG_BT_NIMBLE_ROLE_OBSERVER", False),
        ("CONFIG_BT_NIMBLE_SM_LEGACY", True),
        ("CONFIG_BT_NIMBLE_SM_SC", True),
        ("CONFIG_BT_NIMBLE_NVS_PERSIST", True),
    ]:
        add_idf_sdkconfig_option(opt, val)
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_MAX_BONDS", config[CONF_MAX_BONDS])
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_MAX_CONNECTIONS", config[CONF_MAX_CONNECTIONS])
    add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_BLE_MAX_CONN", config[CONF_MAX_CONNECTIONS])
    add_idf_sdkconfig_option("CONFIG_BT_CTRL_BLE_MAX_ACT", config[CONF_MAX_CONNECTIONS])
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_MAX_CCCDS", 2 * config[CONF_MAX_CONNECTIONS] + 2)
    # Override the NimBLE host task stack (default 4096) to prevent the stack
    # overflow during LE Secure Connections pairing — see CONF_NIMBLE_HOST_TASK_STACK_SIZE.
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE", config[CONF_NIMBLE_HOST_TASK_STACK_SIZE])


_ACTION_SCHEMA = automation.maybe_simple_id({cv.GenerateID(): cv.use_id(AncsComponent)})


@automation.register_action("ancs.clear_bonds", ClearBondsAction, _ACTION_SCHEMA, synchronous=True)
async def clear_bonds_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("ancs.disconnect", DisconnectAction, _ACTION_SCHEMA, synchronous=True)
async def disconnect_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "ancs.request_attributes",
    RequestAttributesAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(AncsComponent),
            cv.Required("uid"): cv.templatable(cv.uint32_t),
        }
    ),
    synchronous=True,
)
async def request_attributes_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config["uid"], args, cg.uint32)
    cg.add(var.set_uid(template_))
    return var
