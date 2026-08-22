/* Empty .init_array section.
   The __init_array_start/__init_array_end boundary symbols that bound the
   array for musl's __libc_start_main() are emitted by the C linker
   (tccelf.c:add_init_array_defines) from the true merged section offsets.
   Defining them here as strong symbols at the same (trailing) offset made
   both resolve to the END of the array, so musl saw an empty array and CTORs
   never ran.  Keep only the section so add_init_array_defines() can find it. */
.section .init_array,"aw"