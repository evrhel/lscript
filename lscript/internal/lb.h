#if !defined(LB_H)
#define LB_H

enum
{
	lb_noop = 0x00,

	lb_class = 0x01,
	lb_extends,

	lb_function = 0x10,
	lb_static,
	lb_dynamic,
	lb_interp,
	lb_native,
	lb_global,
	lb_const,
	lb_varying,

	lb_char = 0x80,
	lb_uchar,
	lb_short,
	lb_ushort,
	lb_int,
	lb_uint,
	lb_long,
	lb_ulong,
	lb_bool,
	lb_float,
	lb_double,
	lb_object,
	lb_chararray,
	lb_uchararray,
	lb_shortarray,
	lb_ushortarray,
	lb_intarray,
	lb_uintarray,
	lb_longarray,
	lb_ulongarray,
	lb_boolarray,
	lb_floatarray,
	lb_doublearray,
	lb_objectarray,

	lb_setb = 0xa0,
	lb_setw,
	lb_setd,
	lb_setq,
	lb_setr4,
	lb_setr8,
	lb_seto,
	lb_setv,
	lb_setr,

	lb_ret = 0xa9,
	lb_retb,
	lb_retw,
	lb_retd,
	lb_retq,
	lb_retr4,
	lb_retr8,
	lb_reto,
	lb_retv,
	lb_retr,

	lb_static_call = 0xb3,
	lb_dynamic_call,
	lb_byte,
	lb_word,
	lb_dword,
	lb_qword,
	lb_real4,
	lb_real8,
	lb_value,
	lb_string,
	lb_new,

	lb_add = 0xbe,
	lb_sub,
	lb_mul,
	lb_div,
	lb_mod,

	lb_if = 0xc3,
	lb_elif,
	lb_else,
	lb_end
};

#endif

