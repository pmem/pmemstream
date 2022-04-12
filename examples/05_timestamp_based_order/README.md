# Timestamp based order example

This example is intended as demo for (not yet implemented) feature of [Timestamps with background worker](https://github.com/pmem/pmemstream/issues/78).

## Usage

```sh
./example-05_timestamp_based_order file
```

## Output

```
entry timestamp 0 with data ( produced by thread 1 with index 0)
entry timestamp 1 with data ( produced by thread 1 with index 1)
entry timestamp 2 with data ( produced by thread 1 with index 2)
entry timestamp 3 with data ( produced by thread 1 with index 3)
entry timestamp 4 with data ( produced by thread 1 with index 4)
entry timestamp 5 with data ( produced by thread 2 with index 0)
entry timestamp 6 with data ( produced by thread 2 with index 1)
entry timestamp 7 with data ( produced by thread 2 with index 2)
entry timestamp 8 with data ( produced by thread 2 with index 3)
entry timestamp 9 with data ( produced by thread 2 with index 4)
entry timestamp 10 with data ( produced by thread 2 with index 5)
entry timestamp 11 with data ( produced by thread 2 with index 6)
entry timestamp 12 with data ( produced by thread 2 with index 7)
entry timestamp 13 with data ( produced by thread 2 with index 8)
entry timestamp 14 with data ( produced by thread 0 with index 0)
entry timestamp 15 with data ( produced by thread 0 with index 1)
entry timestamp 16 with data ( produced by thread 0 with index 2)
entry timestamp 17 with data ( produced by thread 0 with index 3)
entry timestamp 18 with data ( produced by thread 0 with index 4)
entry timestamp 19 with data ( produced by thread 0 with index 5)
entry timestamp 20 with data ( produced by thread 0 with index 6)
entry timestamp 21 with data ( produced by thread 0 with index 7)
entry timestamp 22 with data ( produced by thread 0 with index 8)
entry timestamp 23 with data ( produced by thread 0 with index 9)
entry timestamp 24 with data ( produced by thread 2 with index 9)
entry timestamp 25 with data ( produced by thread 1 with index 5)
entry timestamp 26 with data ( produced by thread 1 with index 6)
entry timestamp 27 with data ( produced by thread 1 with index 7)
entry timestamp 28 with data ( produced by thread 1 with index 8)
entry timestamp 29 with data ( produced by thread 1 with index 9)
```
