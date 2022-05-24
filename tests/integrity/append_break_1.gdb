set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on
set disable-randomization off

# tear after "timestamp acquire" in "publish"

# go into "publish" part and wait for 'timestamp' to be acquired
b pmemstream_acquire_timestamp
r
finish
n 2

# it's 4th append already, so timestamp should be equal to 4
print timestamp
print entry.offset
print destination

# and now watch for memcpy to take place (at best use address offset >512)
# ...to tear entry apart (with timestamp not yet committed)
watch *((uint8_t *)destination+600)
c
q
