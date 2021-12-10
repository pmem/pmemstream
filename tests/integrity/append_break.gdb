set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on

# go to memcpy in append (into publishing part)
b pmemstream_publish
r
b span_create_entry
c
#
# Unfortunately on some systems (various gdb ver.) there's no way to
# jump straight to the stream->memset line, so we go there step-by-step.
s 9
print dest
info line

# watch for memcpy take place (address offset should be >512)
watch *((uint8_t *)dest)
c
q
