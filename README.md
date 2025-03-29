In rust write a little speed bench program that uses multicore. It should count from zero to ulongword (32bit) simultanously on all cores.
It should write what it will do, when it starts, when it ends and how long it took.

It should check for how many cores there is. If there are logical cores available it should run the benchmark twice. Once with only real cores and ones with both real and logical.
