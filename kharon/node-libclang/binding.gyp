{
  "targets": [
    {
      "target_name": "liblibclang",
      "sources": [ "./src/node-libclang.cc" ],
      "libraries": ["-lclang"],
      "cflags": ["-I/usr/local/include"],
    }
  ]
}