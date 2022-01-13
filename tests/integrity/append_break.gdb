set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on
set disable-randomization off

# go to memcpy in append
b pmemstream_reserve
r
# Unfortunately on some systems (various gdb ver.) there's no way to jump straight
# to the stream->memset line, so we go to the point with "reserved_dest" available.
s 1
finish
# reserved_dest before memcpy should be 0'ed
print *(uint8_t *)reserved_dest

# watch for memcpy take place (address offset should be >512)
watch *((uint8_t *)reserved_dest)+600
c
q
