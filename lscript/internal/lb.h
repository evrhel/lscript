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
	lb_setl,
	lb_setq,
	lb_setf,
	lb_setd,
	lb_setv,
	lb_setr,

	lb_ret = 0xa8,
	lb_retb,
	lb_retw,
	lb_retl,
	lb_retq,
	lb_retf,
	lb_retd,
	lb_retv,
	lb_retr,

	lb_static_call = 0xb1,
	lb_dynamic_call,
	lb_byte,
	lb_word,
	lb_lword,
	lb_qword,
	lb_value,

	lb_add = 0xb8,
	lb_sub,
	lb_mul,
	lb_div,
	lb_mod
};

#endif

