set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on
set disable-randomization off

# go into "publish" part and wait for 'timestamp' to be acquired
# (it's 4th append already, so it should be equal to 4)
b pmemstream_acquire_timestamp
r
finish

n 2
print entry.offset
print timestamp
print destination

# and now watch for memcpy to take place (at best use address offset >512)
# ...to tear it apart, so the entry will be invalid
watch *((uint8_t *)destination+600)
c
q
