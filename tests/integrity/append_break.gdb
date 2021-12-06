set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on

# go to memcpy in append (which is next to the second span_get_runtime)
b pmemstream_append
r
info break 1
b pmemstream_span_get_runtime
c
c
finish
info line

# watch for memcpy take place (address offset should be >512)
set $v=entry_rt.data
p $v
watch *($v+600)
c
q
