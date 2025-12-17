# Nul Linux
Nul Linux is an initramfs linux that uses binaries from WSL, specifically Ubuntu 24.04.x LTS from the Windows Store.\
You can `git clone` this repository and run `./build.sh` to install all required dependencies, and complete all steps to obtaining a functional and bootable `nullinux.iso` file.

Following steps will be done during the execution of `./build.sh`:
1. [**dependencies**] The package manager `apt` will be run to obtain dependencies, for privileges, the command is automatically run with `sudo`.
2. [**getlinux**] Then the script will download a version of Linux pinned in the file `tools/getlinux.sh` and copy a customized `.config` file from the `templates` folder. Finally on this step, the linux kernel will be compiled using all cores, determined by the command `nproc`.
3. [**mkrootfs**] The `rootfs` folder will be deleted, if it exists, and a new one is created from files contained in `templates`.
4. [**compiletools**] A script to compile tools will be run next, to compile the `init` program, as well as other programs, customly made for Nul Linux, specifically `dhcp`, `webserver` and `powerctl`. You can find the source code in the `src` folder.
5. [**filegrab**] This tool uses `config/embed.txt` to embed binaries from the host system (WSL) into the `rootfs` folder, it will also attempt to obtain all dynamically linked lib-files and put them into the expected paths within `rootfs`. You can extend the list in `config/embed.txt` to embed more files from the host system, some packages may require additional files.
6. [**copymodules**] All expected and compiled modules that were created during `getlinux` will be copied from the `linux` folder, into the rootfs folder.
7. [**getkbd**] This script will download a file named `kbd_2.7.1.orig.tar.gz` which contains a kbd folder, that contains keymaps for various languages, and place it in the `rootfs`.
8. [**timezone**] Copies the timezone files from the host environment into the rootfs.
9. [**createinitcpio**] This will create a list of all files in the `rootfs` and use the `cpio` program to create an init.cpio file. 
10. [**grubiso**] This script creates a bootable file using the Grub2 bootloader. And place the `nullinux.iso` file into the main nullinux folder.
11. [**qemutest**] That is the final test, that shows you if the `iso` file you created works, you will see a qemu window open, and the linux booting. You can turn it off using the `poweroff` command. You can also attempt to setup persistent storage, as this test will include a persistent storage, stored in `temp` named `persistent.img`.

## What is Nul Linux?
Nul Linux is an initramfs Linux, which is by default ran in RAM, and does not persist changes in storage, however using a the provided tool `persistentsetup`, you can create an auto-mount partition, which will become /root, that becomes persistent upon start. The persistent drive is recommended to be of type `exFAT` and should be labelled `NHOME` (Nul Home). 

With this environment you can make a custom mini Linux by including tools you want, with full control over the environment, because of it's minimalistic nature. Include packages you like, tweak the Linux kernel, bake scripts into it. And flash that mini Linux to USB drives, either through tools like Rufus or by copying the ISO contents into a `FAT32` partition, the size of `64 MB`.

You can make changes to the tools, and the files embedded, and simply update that ISO by running `./build.sh`. Please be aware, that when running `./build.sh` the `rootfs` folder will be deleted. To make changes, use the `templates` folder.

This utility is meant for administrators, to perform tasks, possibly related to data rescue or restoring access to lost systems.

Nul Linux also provides some tools, which the next section will be about.

# Tools

## help
This script overwrites the default help command and shows you the last known Git `HEAD`, which can be useful to determine what is included from this project, essentially tracking and comparing bugs.

The main purpose of this script is to show which custom tools are available within this release.

## bioskey
With the `bioskey` command you can quickly see the Windows product key stored in the BIOS. If no product key is available, you'll be informed accordingly.

Example use: `bioskey >> keys.txt` (Creates the file if it doesn't exist and appends the Windows key to keys.txt)

## letmein
Letmein is a tool designed to quickly swap a file in Windows, which causes the on-screen keyboard to become a command prompt with System privileges, it does this in a reversible way by keeping the original file.

It can only be done if following security measures weren't done:
- Windows is encrypted with BitLocker
- Secure Boot is enabled, and BIOS password is set

If you want to protect against such an attack, encrypt Windows, and make sure a BIOS password is set.

**Only** run this tool if Windows is **not** hibernating.\
To make sure this is the case, start up Windows, and hold the `Shift` key while powering the computer off from the logon screen. Wait until the computer is fully turned off, then release the `Shift` key.

When it ran successfully, you'll see the text "Login cmd is now enabled", running the same command again, will reverse the changes, this can be done even after rebooting and using the command prompt, because it detects the state of these files, based on the presence of a file made on the Windows system.

Please be aware, that this trick can be picked up by security vendors, only use this if you're permitted to do so.

Once the login cmd is enabled, you can restart by typing "reboot" and within the Windows logon screen, you can click on the accessibility tools, and open the on-screen keyboard, which will instead spawn a command prompt.

This command prompt runs as `SYSTEM` user, you can use tools like `net user` to create new users, change local user passwords without knowing them. Or on Windows domain controllers, you can even change the global domain Administrator password, which will also automatically replicate to other domain controllers.

Note about this tool: This trick is well known, and can be done even with simple tools such as a Windows installation stick; the only reason this is not patched, is because to be able to get in, requires the system to be vulnerable in the first place. Either due to lacking protection in BIOS, or due to lacking protection on the OS-level (encryption).
```
Usage: letmein [options]

This command enables a command prompt with system privileges on logon screen
for Microsoft Windows. You can run it again, to undo the changes.

Make sure to shutdown the Windows system by holding shift key while powering down before using this tool.
Hibernation prevents accessing the disk safely.

Options:
  -h, --help           Show this help message
  -t, --target         Specify the target (example: /dev/sda3)
  -u, --undirty        Removes dirty bit from ntfs during runtime
```

## map_bitlocker
This is a helper tool, made to allow opening BitLocker drives, which will appear as block devices in /dev/mapper.

Under the hood it uses `cryptsetup` to open the BitLocker device, as well as modules compiled during the creation of the linux kernel.

```
Usage: map_bitlocker [options]

This tool allows mapping bitlocker drives using a keyfile.
Make sure you close the bitlocker drive with -c when done.

Options:
  -h, --help           Show this help message
  -t, --target         Specify the target (example: /dev/sda3)
  -m, --mount          Mount directory (example: /mnt/)
  -k, --keys           Key file (example: /root/keys.txt)
  -c, --close          Close mapped device (example: sda3)
  -u, --undirty        Removes dirty bit from ntfs during runtime
```

## winfind
This utility finds the block device where Windows is stored, useful if you want to mount a Windows system onto a mount directory.

Example use: `mount -t ntfs3 /dev/sda3 /mnt`

Please be aware, that without `-t ntfs3` it will automatically try to use another ntfs driver, it won't work because it's not enabled in the kernel.

```
Usage: winfind [options]

This command finds Windows on NTFS disks.
If you ran this tool on a hibernating Windows, it may flag the partition as dirty.

Options:
  -h, --help           Show this help message
  -n, --nobitlocker    Ignores BitLocker partitions
  -t, --target         Specify the target (example: /dev/sda3)
  -u, --undirty        Removes dirty bit from ntfs during runtime
```

## persistentsetup
You will be prompted during boot that you can run this command to enable persistent storage, if you have an exFAT partition with the name `NHOME`.

It will copy the /root folder contents, and setup a `config` folder, where you can do configurations that will persist.

## loadkeys

This command loads another keyboard layout. \
Example use: `loadkeys de`

```
Specify language file, eg: de, us, de-latin1..
```

## passwd
This script changes the password, running it will prompt for a new root password.\
Do this if you want to access the system via SSH.

Be aware, that on a persistent setup, the password will be stored in the `NHOME` drive, which would be easy for an attacker to bruteforce.

## sshserver
This script generates required keys for the SSH server to work and runs sshd.

It will also function as an SFTP server.

## webserver
This program runs a webserver in the foreground, useful if you want to expose a folder through port 80.

It will also allow directory browsing, and features the ability to detect `index.html` files.

Example: `webserver -p 8080 -u access -P secretpassword`

```
Usage: webserver [-p port] [-u user -P pass]
```

Please be aware, that accessing the webserver with basic authentication, could send the password in plaintext through the network.

## poweroff

Changes directory to /, so `root` no longer is used, then it'll try to unmount it through `powerctl`. The `powerctl` program prepares the system, so the `init` program can perform the shutdown.

Turns the system off.

## reboot

Changes directory to /, so `root` no longer is used, then it'll try to unmount it through `powerctl`. The `powerctl` program prepares the system, so the `init` program can perform the reboot.

Reboots the system.

# Configuring Nul Linux

## Configure a timezone

If you are in ramdisk mode, you can set the timezone, by creating a symlink:\
`ln -s /usr/share/zoneinfo/Europe/Paris /etc/localtime`

You can set a persistent timezone by creating a file in persistent mode:\
`echo Europe/Paris > ~/config/timezone`

## Setting the keyboard layout

In ramdisk mode, you can load a keyboard layout by typing:\
`loadkeys fr` (Example for french keyboard layout.)

In persistent mode, you can add following line in `~/config/user.autostart.sync.txt`:\
`loadkeys de` (Example for german keyboard layout.)

# Build Options

Here's the help of the `build.sh` script:

```
Usage: ./build.sh [options]

Installs required dependencies, and crafts an ISO file in the same directory as $(basename $0).

Options:
  -h, --help                   Shows the help of the build tool.
  -q, --quiet                  Skips running qemu, useful if you just want to obtain the ISO file.
  -s, --skip-qemu-install      Skips the installation of qemu.
  -i, --install-qemu           Useful if you initially skipped the installation of qemu.
  -x, --extract                Extracts the generated ISO file to an output directory.
```

In case you want to just build the ISO and move it to your user profile folder, you can run `build.sh` like this:\
`./build.sh -q -s -x /mnt/c/Users/MyProfile/`

# About distribution
Due to the fact that this only contains partial binaries of packages, I want to clarify to *not* distribute ISOs.

This project will never distribute an ISO file, build it yourself, it's easy.


