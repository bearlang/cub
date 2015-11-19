{
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
              '-g3'
            ]
          }]
        ],
        'defines': ['DEBUG'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1
          }
        }
      },
      'Release': {
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
              '-O3'
            ]
          }]
        ],
        'defines': ['NODEBUG'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0,
          }
        }
      }
    }
  },
  'targets': [{
    'target_name': 'cub',
    'type': 'executable',
    'conditions': [
      ['OS=="linux"', {
        'cflags': [
          '-Wall', '-Wextra', '-std=c11', '-fdiagnostics-color'
        ],
        'defines': ['_POSIX_SOURCE', '_GNU_SOURCE']
      }]
    ],
    'link_settings': {
      'libraries': [
        '-lm'
      ],
    },
    'include_dirs': [
      '.'
    ],
    'sources': [
      'expression/assign.c',
      'expression/call.c',
      'expression/cast.c',
      'expression/compare.c',
      'expression/function.c',
      'expression/get-field.c',
      'expression/get-index.c',
      'expression/get-symbol.c',
      'expression/identity.c',
      'expression/literal.c',
      'expression/logic.c',
      'expression/native.c',
      'expression/negate.c',
      'expression/new.c',
      'expression/new-array.c',
      'expression/not.c',
      'expression/numeric.c',
      'expression/postfix.c',
      'expression/shift.c',
      'expression/str-concat.c',
      'expression/ternary.c',
      'statement/block.c',
      'statement/break.c',
      'statement/class.c',
      'statement/continue.c',
      'statement/define.c',
      'statement/expression.c',
      'statement/function.c',
      'statement/if.c',
      'statement/let.c',
      'statement/loop.c',
      'statement/return.c',
      'statement/type.c',
      'xalloc.c',
      'token.c',
      'buffer.c',
      'stream.c',
      'lex.c',
      'type.c',
      'parse-common.c',
      'parse-detect-ambiguous.c',
      'parse.c',
      'analyze-symbol.c',
      'analyze-type.c',
      'analyze.c',
      'generate-common.c',
      'generate-block.c',
      'generate.c',
      'optimize.c',
      'llvm-backend/llvm-backend.c',
      'compile.c'
    ],
    'actions': [{
      'variables': {
        'lib_name': 'core'
      },
      'action_name': 'library_include',
      'inputs': [
        'lib/<(lib_name).cub'
      ],
      'outputs': [
        'out/lib/<(lib_name).h'
      ],
      'action': [
        'xxd', '-i', 'lib/<(lib_name).cub', 'out/lib/<(lib_name).h'
      ],
      'message': 'Including core library'
    }]
  }]
}
