# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Brian Towles

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text
from esphome.const import CONF_RESTORE_VALUE

from . import AncsComponent, ancs_ns

CONF_ANCS_ID = "ancs_id"

AncsNameSuffixText = ancs_ns.class_("AncsNameSuffixText", text.Text, cg.Component)

CONFIG_SCHEMA = (
    text.text_schema(AncsNameSuffixText, mode="TEXT")
    .extend(
        {
            cv.GenerateID(CONF_ANCS_ID): cv.use_id(AncsComponent),
            cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ANCS_ID])
    var = await text.new_text(config, min_length=0, max_length=29, pattern=None)
    await cg.register_component(var, config)
    cg.add(var.set_parent(parent))
    cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))
