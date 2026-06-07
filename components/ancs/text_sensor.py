# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Brian Towles

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import AncsComponent

CONF_ANCS_ID = "ancs_id"
CONF_CONNECTED_DEVICE = "connected_device"
CONF_LAST_TITLE = "last_title"
CONF_LAST_MESSAGE = "last_message"
CONF_LAST_APP_ID = "last_app_id"
CONF_LAST_CALLER = "last_caller"
CONF_ADVERTISED_NAME = "advertised_name"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ANCS_ID): cv.use_id(AncsComponent),
        cv.Optional(CONF_CONNECTED_DEVICE): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_LAST_TITLE): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_LAST_MESSAGE): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_LAST_APP_ID): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_LAST_CALLER): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_ADVERTISED_NAME): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:bluetooth",
        ),
    }
)

_SETTERS = {
    CONF_CONNECTED_DEVICE: "set_connected_device_text_sensor",
    CONF_LAST_TITLE: "set_last_title_text_sensor",
    CONF_LAST_MESSAGE: "set_last_message_text_sensor",
    CONF_LAST_APP_ID: "set_last_app_id_text_sensor",
    CONF_LAST_CALLER: "set_last_caller_text_sensor",
    CONF_ADVERTISED_NAME: "set_advertised_name_text_sensor",
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ANCS_ID])
    for key, setter in _SETTERS.items():
        if key in config:
            s = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(parent, setter)(s))
