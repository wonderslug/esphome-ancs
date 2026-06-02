# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Brian Towles

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import AncsComponent

CONF_ANCS_ID = "ancs_id"
CONF_CONNECTED = "connected"
CONF_CALL_ACTIVE = "call_active"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ANCS_ID): cv.use_id(AncsComponent),
        cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_CALL_ACTIVE): binary_sensor.binary_sensor_schema(),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ANCS_ID])
    if CONF_CONNECTED in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_CONNECTED])
        cg.add(parent.set_connected_binary_sensor(s))
    if CONF_CALL_ACTIVE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_CALL_ACTIVE])
        cg.add(parent.set_call_active_binary_sensor(s))
