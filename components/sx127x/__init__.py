from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_PAYLOAD,
    CONF_TRIGGER_ID,
)
from esphome.components import spi

MULTI_CONF = True
CODEOWNERS = ["@bouttier"]
DEPENDANCIES = ["spi"]

CONF_DIO0_PIN = "dio0_pin"
CONF_LORA_BANDWIDTH = "lora_bandwidth"
CONF_LORA_CODINGRATE = "lora_codingrate"
CONF_LORA_CRC = "lora_crc"
CONF_LORA_INVERT_IQ = "lora_invert_iq"
CONF_LORA_SYNCWORD = "lora_syncword"
CONF_LORA_PREAMBLE_LENGTH = "lora_preamble_length"
CONF_LORA_SPREADING_FACTOR = "lora_spreading_factor"
CONF_ON_PACKET = "on_packet"
CONF_RESET_PIN = "reset_pin"
CONF_RF_FREQUENCY = "rf_frequency"
CONF_TX_POWER = "tx_power"
CONF_PA_PIN = "pa_pin"
CONF_OPMOD = "opmod"
CONF_QUEUE_LEN = "queue_len"

sx127x_ns = cg.esphome_ns.namespace("sx127x")

SX127XComponent = sx127x_ns.class_(
    "SX127XComponent",
    cg.Component,
    spi.SPIDevice,
)
SX127XSendAction = sx127x_ns.class_(
    "SX127XSendAction",
    automation.Action,
)
SX127XSetOpmodAction = sx127x_ns.class_(
    "SX127XSetOpmodAction",
    automation.Action,
    cg.Parented.template(SX127XComponent),
)
SX127XRecvTrigger = sx127x_ns.class_(
    "SX127XRecvTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.uint8)),
)
SX127XIsTransmittingCondition = sx127x_ns.class_(
    "SX127XIsTransmittingCondition",
    automation.Condition,
)
SX127XSpreadingFactor = cg.global_ns.enum("sx127x_sf_t")
SX127XCodeRate = cg.global_ns.enum("sx127x_cr_t")
SX127XBandwidth = cg.global_ns.enum("sx127x_bw_t")
SX127XMode = cg.global_ns.enum("sx127x_mode_t")
SX127XPaPin = cg.global_ns.enum("sx127x_pa_pin_t")


def validate_raw_data(value):
    if isinstance(value, str):
        return list(value.encode("utf-8"))
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


SF = {
    "SF6": SX127XSpreadingFactor.SX127x_SF_6,
    "SF7": SX127XSpreadingFactor.SX127x_SF_7,
    "SF8": SX127XSpreadingFactor.SX127x_SF_8,
    "SF9": SX127XSpreadingFactor.SX127x_SF_9,
    "SF10": SX127XSpreadingFactor.SX127x_SF_10,
    "SF11": SX127XSpreadingFactor.SX127x_SF_11,
    "SF12": SX127XSpreadingFactor.SX127x_SF_12,
}
CR = {
    "CR_4_5": SX127XCodeRate.SX127x_CR_4_5,
    "CR_4_6": SX127XCodeRate.SX127x_CR_4_6,
    "CR_4_7": SX127XCodeRate.SX127x_CR_4_7,
    "CR_4_8": SX127XCodeRate.SX127x_CR_4_8,
}
BW = {
    7800: SX127XBandwidth.SX127x_BW_7800,
    10400: SX127XBandwidth.SX127x_BW_10400,
    15600: SX127XBandwidth.SX127x_BW_15600,
    20800: SX127XBandwidth.SX127x_BW_20800,
    31250: SX127XBandwidth.SX127x_BW_31250,
    41700: SX127XBandwidth.SX127x_BW_41700,
    62500: SX127XBandwidth.SX127x_BW_62500,
    125000: SX127XBandwidth.SX127x_BW_125000,
    250000: SX127XBandwidth.SX127x_BW_250000,
    500000: SX127XBandwidth.SX127x_BW_500000,
}
OPMOD = {
    "sleep": SX127XMode.SX127x_MODE_SLEEP,
    "standby": SX127XMode.SX127x_MODE_STANDBY,
    "rx": SX127XMode.SX127x_MODE_RX_CONT,
}

PA_PIN = {
    "RFO": SX127XPaPin.SX127x_PA_PIN_RFO,
    "PA_BOOST": SX127XPaPin.SX127x_PA_PIN_BOOST,
}

# LoRa Radio Parameters
CONF_DEFAULT_RF_FREQUENCY = 915e6  # Hz
CONF_DEFAULT_TX_POWER = 4  # dBm
CONF_DEFAULT_PA_PIN = "RFO"
CONF_DEFAULT_LORA_BANDWIDTH = 125e3  # Hz
CONF_DEFAULT_LORA_SPREADING_FACTOR = "SF7"  # [SF6..SF12]
CONF_DEFAULT_LORA_CODINGRATE = "CR_4_5"  # [4/5, 4/6, 4/7, 4/8]
CONF_DEFAULT_LORA_PREAMBLE_LENGTH = 8
CONF_DEFAULT_LORA_CRC = True
CONF_DEFAULT_LORA_IQ_INVERSION = False
CONF_DEFAULT_LORA_SYNCWORD = 0x12
CONF_DEFAULT_OPMOD = "standby"
CONF_DEFAULT_QUEUE_LEN = 10

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SX127XComponent),
            cv.Optional(CONF_NAME): cv.string,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DIO0_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(
                CONF_RF_FREQUENCY, default=CONF_DEFAULT_RF_FREQUENCY
            ): cv.int_range(min=137000000, max=1020000000),
            cv.Optional(CONF_TX_POWER, default=CONF_DEFAULT_TX_POWER): cv.int_,
            cv.Optional(CONF_PA_PIN, default=CONF_DEFAULT_PA_PIN): cv.enum(PA_PIN),
            cv.Optional(
                CONF_LORA_BANDWIDTH, default=CONF_DEFAULT_LORA_BANDWIDTH
            ): cv.enum(BW),
            cv.Optional(
                CONF_LORA_SPREADING_FACTOR, default=CONF_DEFAULT_LORA_SPREADING_FACTOR
            ): cv.enum(SF),
            cv.Optional(CONF_LORA_CRC, default=CONF_DEFAULT_LORA_CRC): cv.boolean,
            cv.Optional(
                CONF_LORA_CODINGRATE, default=CONF_DEFAULT_LORA_CODINGRATE
            ): cv.enum(CR),
            cv.Optional(
                CONF_LORA_PREAMBLE_LENGTH, default=CONF_DEFAULT_LORA_PREAMBLE_LENGTH
            ): cv.int_range(6, 65535),
            cv.Optional(
                CONF_LORA_SYNCWORD, default=CONF_DEFAULT_LORA_SYNCWORD
            ): cv.uint8_t,
            cv.Optional(
                CONF_LORA_INVERT_IQ, default=CONF_DEFAULT_LORA_IQ_INVERSION
            ): cv.boolean,
            cv.Optional(CONF_OPMOD, default=CONF_DEFAULT_OPMOD): cv.enum(OPMOD),
            cv.Optional(CONF_QUEUE_LEN, default=CONF_DEFAULT_QUEUE_LEN): cv.int_range(
                0, 100
            ),
            cv.Optional(CONF_ON_PACKET): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SX127XRecvTrigger),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        spi.spi_device_schema(
            cs_pin_required=True, default_data_rate=4e6, default_mode="mode0"
        )
    )
)


async def to_code(config):
    # We require this feature to be merged: https://github.com/dernasherbrezon/sx127x/pull/20
    # cg.add_library("dernasherbrezon/sx127x", "4.0.1")
    cg.add_library(
        name="sx127x",
        version=None,
        repository="https://github.com/bouttier/sx127x.git#callback_context",
    )

    if config[CONF_PA_PIN] == "RFO":
        if config[CONF_TX_POWER] < -4 or config[CONF_TX_POWER] > 15:
            raise cv.Invalid("With RFO pin, TX power must be in range [-4,15]")
    elif config[CONF_PA_PIN] == "PA_BOOST":
        if (
            config[CONF_TX_POWER] < 2
            or config[CONF_TX_POWER] > 20
            or config[CONF_TX_POWER] in [18, 19]
        ):
            raise cv.Invalid(
                "With PA_BOOST pin, TX power must be in range [2,17] or be 20"
            )

    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)

    reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset_pin))
    if CONF_DIO0_PIN in config:
        dio0_pin = await cg.gpio_pin_expression(config[CONF_DIO0_PIN])
        cg.add(var.set_dio0_pin(dio0_pin))

    cg.add(var.set_rf_frequency(config[CONF_RF_FREQUENCY]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))
    cg.add(var.set_pa_pin(config[CONF_PA_PIN]))
    cg.add(var.set_lora_bandwidth(config[CONF_LORA_BANDWIDTH]))
    cg.add(var.set_lora_spreading_factor(config[CONF_LORA_SPREADING_FACTOR]))
    cg.add(var.set_lora_enable_crc(config[CONF_LORA_CRC]))
    cg.add(var.set_lora_codingrate(config[CONF_LORA_CODINGRATE]))
    cg.add(var.set_lora_preamble_length(config[CONF_LORA_PREAMBLE_LENGTH]))
    cg.add(var.set_lora_syncword(config[CONF_LORA_SYNCWORD]))
    cg.add(var.set_lora_invert_iq(config[CONF_LORA_INVERT_IQ]))
    cg.add(var.set_opmod(config[CONF_OPMOD]))
    cg.add(var.set_queue_len(config[CONF_QUEUE_LEN]))

    for conf in config.get(CONF_ON_PACKET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (cg.std_vector.template(cg.uint8), "payload"),
                (cg.float_, "snr"),
                (cg.int16, "rssi"),
            ],
            conf,
        )


LORA_SEND_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(SX127XComponent),
        cv.Required(CONF_PAYLOAD): cv.templatable(validate_raw_data),
    },
    key=CONF_PAYLOAD,
)


LORA_SET_OPMOD_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(SX127XComponent),
        cv.Required(CONF_OPMOD): cv.templatable(cv.enum(OPMOD)),
    },
    key=CONF_OPMOD,
)


LORA_CONDITION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SX127XComponent),
    }
)


@automation.register_action(
    "sx127x.send",
    SX127XSendAction,
    LORA_SEND_ACTION_SCHEMA,
)
async def sx127x_send_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template = await cg.templatable(
        config[CONF_PAYLOAD], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_payload(template))

    return var


@automation.register_action(
    "sx127x.set_opmod",
    SX127XSetOpmodAction,
    LORA_SET_OPMOD_ACTION_SCHEMA,
)
async def sx127x_mode_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template = await cg.templatable(
        config[CONF_OPMOD],
        args,
        SX127XMode,
    )
    cg.add(var.set_opmod(template))
    return var


@automation.register_condition(
    "sx127x.is_transmitting",
    SX127XIsTransmittingCondition,
    LORA_CONDITION_SCHEMA,
)
async def sx127x_is_transmitting_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
