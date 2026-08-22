/* Empty .init_array/.fini_array sections.  The boundary symbols
   (__init_array_start/end, __fini_array_start/end) are emitted by the C
   linker (tccelf.c:add_init_array_defines) from true merged offsets; see
   src/posix/musl-nt64/crt/init_array.s for the rationale. */
.section .init_array,"aw"
.section .fini_array,"aw"
