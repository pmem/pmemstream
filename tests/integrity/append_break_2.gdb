set width 0
set height 0
set verbose off
set confirm off
set breakpoint pending on
set disable-randomization off

# tear after entry is written, after "publish"

# wait for entry (with acquired timestamp) to be written to pmem,
# but don't allow to update the 'persisted_timestamp'
b pmemstream_async_wait_persisted
r
q
