# Chapter 30: Testing with QEMU

QEMU provides an excellent environment for testing VOS without requiring real hardware.

## QEMU Basics

### What is QEMU?

QEMU (Quick EMUlator) is a free and open-source emulator that performs hardware virtualization. For OS development, it provides:

- Full x86 system emulation
- Various device emulation (VGA, disk, network)
- Debugging features
- Snapshot/restore capabilities

### Installing QEMU

```bash
# Ubuntu/Debian
sudo apt install qemu-system-x86

# Fedora
sudo dnf install qemu-system-x86

# Arch Linux
sudo pacman -S qemu-system-x86

# macOS
brew install qemu
```

## Running VOS

### Basic Execution

```bash
make run
```

This runs:
```bash
qemu-system-i386 -cdrom vos.iso -vga none \
    -device bochs-display,xres=1920,yres=1080
```

### Custom Resolution

```bash
# 720p
make run QEMU_XRES=1280 QEMU_YRES=720

# 1080p (default)
make run QEMU_XRES=1920 QEMU_YRES=1080

# 4K
make run QEMU_XRES=3840 QEMU_YRES=2160
```

### Debug Mode

```bash
make debug
```

Adds `-d int -no-reboot` for interrupt logging and no auto-reboot on crash.

## Display Options

### Bochs Display (Recommended)

```bash
qemu-system-i386 -cdrom vos.iso -vga none \
    -device bochs-display,xres=1920,yres=1080
```

Best for high resolution framebuffer support.

### Standard VGA

```bash
qemu-system-i386 -cdrom vos.iso -vga std
```

Limited to 800x600 or lower.

### Cirrus VGA

```bash
qemu-system-i386 -cdrom vos.iso -vga cirrus
```

Legacy VGA compatibility.

## Disk Images

### FAT16 Persistent Disk

VOS supports a persistent FAT16 disk mounted at `/disk`:

```bash
# Create disk image
truncate -s 256M vos-disk.img
mkfs.fat -F 16 -n VOSDISK vos-disk.img

# Run with disk
qemu-system-i386 -cdrom vos.iso -vga none \
    -device bochs-display,xres=1920,yres=1080 \
    -drive file=vos-disk.img,format=raw,if=ide
```

### IDE/ATA Configuration

```bash
# Primary master (hda)
-drive file=disk.img,format=raw,if=ide,index=0

# Primary slave (hdb)
-drive file=disk2.img,format=raw,if=ide,index=1
```

## Memory Configuration

### RAM Size

```bash
# 128 MB RAM (default)
qemu-system-i386 -cdrom vos.iso -m 128

# 256 MB RAM
qemu-system-i386 -cdrom vos.iso -m 256

# 512 MB RAM
qemu-system-i386 -cdrom vos.iso -m 512
```

## Serial Console

VOS outputs debug information to COM1. Capture it with:

### Console Output

```bash
qemu-system-i386 -cdrom vos.iso -serial stdio
```

### Log to File

```bash
qemu-system-i386 -cdrom vos.iso -serial file:serial.log
```

### Telnet Access

```bash
qemu-system-i386 -cdrom vos.iso -serial telnet:localhost:4321,server,nowait

# Connect from another terminal
telnet localhost 4321
```

## Debugging with GDB

### Start QEMU with GDB Server

```bash
qemu-system-i386 -cdrom vos.iso -s -S
```

- `-s`: Enable GDB server on port 1234
- `-S`: Start paused (wait for GDB)

### Connect GDB

```bash
gdb build/kernel.bin
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

### Useful GDB Commands

```gdb
# Set breakpoint
break kernel_main
break *0x100000

# Step
step      # Step into
next      # Step over
stepi     # Single instruction

# Examine
info registers
x/10x $esp    # Examine stack
x/10i $eip    # Examine instructions

# Memory
print variable
x/s 0x1000    # Examine string

# Continue
continue
```

## Debugging Features

### Interrupt Logging

```bash
qemu-system-i386 -cdrom vos.iso -d int
```

Logs all interrupts to stderr.

### CPU State

```bash
qemu-system-i386 -cdrom vos.iso -d cpu
```

### All Logging

```bash
qemu-system-i386 -cdrom vos.iso -d int,cpu,exec -D qemu.log
```

### QEMU Monitor

Press `Ctrl+Alt+2` to access QEMU monitor:

```
(qemu) info registers
(qemu) info mem
(qemu) info block
(qemu) xp /10x 0x100000
(qemu) quit
```

## Performance Testing

### CPU Cores

```bash
# Single CPU (default)
qemu-system-i386 -cdrom vos.iso -smp 1

# Multiple CPUs (for future SMP support)
qemu-system-i386 -cdrom vos.iso -smp 4
```

### Enable KVM (Linux)

```bash
qemu-system-i386 -cdrom vos.iso -enable-kvm
```

KVM provides near-native performance but requires:
- Linux host
- VT-x/AMD-V support
- Proper permissions

### Disable KVM

```bash
qemu-system-i386 -cdrom vos.iso -no-kvm
```

## Network (Future)

When VOS gains network support:

```bash
# User-mode networking
qemu-system-i386 -cdrom vos.iso -net nic -net user

# TAP networking (Linux, requires setup)
qemu-system-i386 -cdrom vos.iso -net nic -net tap,ifname=tap0
```

## Snapshots

### Save State

In QEMU monitor (`Ctrl+Alt+2`):
```
(qemu) savevm snapshot1
```

### Load State

```
(qemu) loadvm snapshot1
```

### Command Line

```bash
# Load existing snapshot
qemu-system-i386 -cdrom vos.iso -loadvm snapshot1
```

## Common Issues

### "No bootable device"

- Check ISO path is correct
- Verify GRUB configuration
- Ensure kernel.bin exists in ISO

### Black Screen

- Try different display: `-vga std`
- Check if framebuffer initialization fails
- Look at serial output

### Triple Fault / Reboot Loop

- Add `-no-reboot` to see crash
- Add `-d int` to see interrupts
- Check GDT/IDT setup
- Verify stack setup

### Performance Issues

- Enable KVM if available
- Reduce resolution
- Check for infinite loops

## Testing Checklist

### Basic Boot

- [ ] Kernel loads
- [ ] Multiboot info received
- [ ] GDT/IDT set up
- [ ] Interrupts enabled
- [ ] Timer ticking

### Console

- [ ] Screen clears
- [ ] Text displays
- [ ] Cursor works
- [ ] Scrolling works

### Keyboard

- [ ] Keys register
- [ ] Shift/Ctrl work
- [ ] Special keys (arrows, function keys)

### Filesystem

- [ ] Initramfs loads
- [ ] Files readable
- [ ] FAT disk mounts
- [ ] Write works

### Processes

- [ ] Init runs
- [ ] Shell starts
- [ ] Fork works
- [ ] Exec works
- [ ] Exit works

### Programs

- [ ] Built-in commands work
- [ ] External programs run
- [ ] Arguments passed
- [ ] Exit codes correct

## Automated Testing

### Script Example

```bash
#!/bin/bash
# test_vos.sh

TIMEOUT=30

# Build
make clean && make || exit 1

# Run with timeout
timeout $TIMEOUT qemu-system-i386 \
    -cdrom vos.iso \
    -vga none \
    -device bochs-display,xres=800,yres=600 \
    -serial file:test.log \
    -no-reboot \
    -display none &

PID=$!
sleep $TIMEOUT
kill $PID 2>/dev/null

# Check results
if grep -q "PANIC" test.log; then
    echo "FAIL: Kernel panic detected"
    exit 1
fi

echo "PASS: No panics detected"
```

## Summary

QEMU testing provides:

1. **Easy execution** with make targets
2. **Display options** for different resolutions
3. **Disk support** for persistent storage
4. **Serial console** for debug output
5. **GDB integration** for debugging
6. **Logging** for interrupt/CPU analysis
7. **Snapshots** for state management
8. **Automation** for CI/CD testing

---

*Previous: [Chapter 29: Build System](29_build.md)*
*Next: [Chapter 31: Project Structure](31_structure.md)*
