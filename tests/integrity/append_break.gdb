set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on

# go to memcpy in append (which is next to the second span_get_runtime)
b pmemstream_append
r
info break 1
b span_create_entry
c
#
# Unfortunately on some systems (various gdb ver.) there's no way to
# jump straight to the stream->memset line, so we go there step-by-step.
b pmemstream_offset_to_ptr
c
finish
info line
s 5
info line

# watch for memcpy take place (address offset should be >512)
watch *((uint8_t *)dest)+600
c
q
