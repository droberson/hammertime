# hammertime

`hammertime` is a proof-of-concept LKM used to force unloading of
other LKMs including those resistant to `rmmod` (e.g., rootkits or EDR
software).

It builds on the technique outlined in https://github.com/MatheuZSecurity/UnhookingLinuxEdr
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

First, build the `canttouchthis.ko` and `hammertime.ko`
LKMs. Installing the relevant compilers, headers, and library is left
as an exercise to the reader:

```sh
git clone git@github.com:droberson/hammertime.git
cd hammertime
make
```

This builds two modules:

- hammertime.ko - a generic force-unloader LKM. It accepts a
  `cleanup_fn=0xaddress` parameter which it will call upon loading,
  triggering the `cleanup_module()` function of the target module. It
  then immediately returns `-ECANCELLED`, causing itself to unload
  cleanly (but throws an error to the user loading it).

- canttouchthis.ko - a generic LKM that does nothing except for
  increase its reference count to prevent easy unloading with `rmmod`.


## Load `canttouchthis`:

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

## Try to unload the module with `rmmod`:

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

## Finally, use `hammertime` to unload it:

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
