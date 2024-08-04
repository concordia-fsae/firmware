<%! from classes.Types import Continuous %>
<%def name="make_packfn(bus, msg)">
static bool pack_${bus.upper()}_${msg.name}(CAN_data_T *message, const uint8_t counter)
{
%for signal in msg.get_non_val_sigs():
  %if signal.condition == "":
    set_${bus.upper()}_${signal.name}(message, ${signal.var}.${signal.member});
  %else:
    set_${bus.upper()}_${signal.name}(message, (${signal.condition}) ? 0x01 : 0x01);
  %endif
%endfor

  %if msg.counter_sig:
    message->u64 |= counter;
  %else:
    (void)counter;
  %endif

    return true;
}
</%def>
\
<%def name="make_setfn(bus, signal)">
  %if signal.continuous == Continuous.continuous:
static inline void set_${bus.upper()}_${signal.name}(CAN_data_T *message, float32_t value)
{
    if (value > ${signal.native_representation.range.max})
    {
        value = ${signal.native_representation.range.max};
    }
    else if (value < ${signal.native_representation.range.min})
    {
        value = ${signal.native_representation.range.min};
    }

    message->u64 |= ((uint64_t)((value - ${signal.offset}) / ${signal.native_representation.resolution}f) & MASK_U${signal.native_representation.bit_width}) << ${signal.start_bit};
}
  %elif signal.condition != "":
static inline void set_${bus.upper()}_${signal.name}(CAN_data_T *message, bool value)
{
    message->u64 |= (uint64_t)value << ${signal.start_bit};
}
  %else:
static inline void set_${bus.upper()}_${signal.name}(CAN_data_T *message, int32_t value)
{
    if (value > ${signal.native_representation.range.max})
    {
        value = ${signal.native_representation.range.max};
    }
    else if (value < ${signal.native_representation.range.min})
    {
        value = ${signal.native_representation.range.min};
    }

    message->u64 |= ((uint64_t)((value - ${signal.offset}) / ${signal.native_representation.resolution}) & MASK_U${signal.native_representation.bit_width}) << ${signal.start_bit};
}
  %endif

</%def>
\
<%def name="make_packtable(bus, msgs, cycle_time)">
const packTable_S ${bus.upper()}_packTable_${cycle_time}ms [] = {
  %for msg in msgs:
    { &pack_${bus.upper()}_${msg.name}, ${hex(msg.id)}, ${msg.length_bytes}U },
  %endfor
};
</%def>
