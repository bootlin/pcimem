# pcimem

 `pcimem` is a `devmem` like tool but for PCI resources access from userspace.
It allows to access BAR content from userspace using a specific access size to
access registers for instance.

## Building:

gengetopt is required to generate the command line. Once installed, just run make:

```
$ make
```

## Example

Reading 4 bytes from PCIe device 07:00.0 BAR 0 at address 0x10 into standard io

```
# pcimem -d 07:00.0 -a 0x10 -s 4 -R 0 -r | hexdump
```

Writing 16 bytes to PCIe device 07:00.0 BAR 0 at address 0x10 from file

```
# pcimem -d 07:00.0 -a 0x10 -s 16 -R 0 -w -f input_file
```