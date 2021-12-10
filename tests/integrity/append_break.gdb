set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on

# go to memcpy in append
b pmemstream_append
r
# Unfortunately on some systems (various gdb ver.) there's no way to
# jump straight to the stream->memset line, so we go there step-by-step.
s 3
finish
info line
print reserved_dest
print *(uint8_t *)reserved_dest

# watch for memcpy take place (address offset should be >512)
watch *((uint8_t *)reserved_dest)+600
c
q
