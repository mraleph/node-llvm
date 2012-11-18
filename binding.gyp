{
  "targets": [
    {
      "target_name": "llvm",
      "sources": [ "src/node-llvm.cc", "src/v8capi.cc", '<(SHARED_INTERMEDIATE_DIR)/generated-bindings.cc' ],
      "dependencies": ['generated-bindings'],
      "conditions": [
        ['OS=="win"', {}, { 'libraries': ['<!@(llvm-config --libs core engine scalaropts)'] }],
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '<!@(llvm-config --cflags)',
              # TODO(vegorov) figure out how to avoid pwd here
              '-I<!@(pwd)/src'
            ]
          }
        }, {
          'cflags': [
            '<!@(llvm-config --cflags)'
          ],
        }]
      ]
    },
    {
      'target_name': 'generated-bindings',
      'type': 'none',
      'variables': {
        'source_files': [
           'src/bindings.cc'
        ],
      },
      'actions': [
        {
          'action_name': 'kharon',
          'inputs': [
            'kharon/kharon.js',
            'kharon/marshalers.js',
            'kharon/trie.js',
            '<@(source_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/generated-bindings.cc',
          ],
          'action': [
            'node',
            'kharon/kharon.js',
            'src/bindings.cc',
            '<@(_outputs)',
            '<!@(llvm-config --cxxflags)'
          ],
        },
      ],
    },
  ]
}