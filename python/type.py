# TODO: map, list, class, function
T_ARRAY    = 0
T_BLOCKREF = 1
T_BOOL     = 2
T_F32      = 3
T_F64      = 4
T_F128     = 5
T_FUNCREF  = 6
T_OBJECT   = 7
T_REF      = 8
T_S8       = 9
T_S16      = 10
T_S32      = 11
T_S64      = 12
T_STRING   = 13
T_U8       = 14
T_U16      = 15
T_U32      = 16
T_U64      = 17
T_VOID     = 18

type_strings = {
  T_ARRAY:    'array',
  T_BLOCKREF: 'blockref',
  T_BOOL:     'bool',
  T_F32:      'f32',
  T_F64:      'f64',
  T_F128:     'f128',
  T_FUNCREF:  'funcref',
  T_OBJECT:   'object',
  T_REF:      'ref',
  T_S8:       's8',
  T_S16:      's16',
  T_S32:      's32',
  T_S64:      's64',
  T_STRING:   'string',
  T_U8:       'u8',
  T_U16:      'u16',
  T_U32:      'u32',
  T_U64:      'u64',
  T_VOID:     'void'
}

def type_to_str(t):
  return type_strings[t]
