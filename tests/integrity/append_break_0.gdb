set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on
set disable-randomization off

# tear after "reserve" (in the middle of the first memcpy)

# go to the first memcpy in "append" - it happens after the "reserve" part
# and it's writing at reserved_dest address.
b pmemstream_reserve
r
finish

# reserved_dest before memcpy should be 0'ed
print *(uint8_t *)reserved_dest

# watch for memcpy take place (address offset should be >512)
watch *((uint8_t *)reserved_dest)+600
c
q
