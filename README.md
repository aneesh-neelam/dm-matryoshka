Matryoshka - Linux Device Mapper
================================

dm-matryoshka is a Linux Device Mapper implementation for Matryoshka.

### Installation:

###### Dependencies:

* Linux kernel source
* gcc
* modprobe
* depmod
* [Optional] X.509 certificate to sign kernel module, required for UEFI Secure Boot

The following are Makefile operations:

###### Build kernel module:

    $ make

###### Sign the module:

Please change the file paths for the public and private X.509 key files in the Makefile.

    $ make sign

###### Install the module for current kernel:

    # make install

###### Load module:

    # make load

###### Unload module:

    # make unload

### Usage:

Using `dmsetup`:

    # dmsetup ...

## Contributing and License

Feel free to create Issues and issue Pull Requests/send Git patches to contribute to this project.

Code is dual-licensed under [GNU General Public License version 2](LICENSE_GPL) and [BSD 3-Clause License](LICENSE_BSD).
