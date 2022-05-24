set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on
set disable-randomization off

# go to timestamp acquire in append to know the current timestamp
b pmemstream_publish
r
# Unfortunately on some systems (various gdb ver.) there's no way to jump straight
# to the stream->memcpy line, so we go to the point where timestamp is acquired.
b pmemstream_acquire_timestamp
c
finish
n 1
# 'timestamp' should be acquired (and equal to 3 - it's 4th append already)
print timestamp
print destination

# and now watch for memcpy take place (address offset should be >512)
# watch *((uint8_t *)destination)+600
# c
q
