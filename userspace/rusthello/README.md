# rusthello

This is a statically linked 32-bit Linux executable for Soso.

Build it from this directory:

```sh
cargo build
```

The configured linker copies a successful debug or release build to
`../bin/rusthello`, ready for inclusion in the initrd.

The `i686-unknown-linux-musl` Rust target must be installed. The linker wrapper
uses `cc` by default; set `CC` if a different C linker driver is needed.
