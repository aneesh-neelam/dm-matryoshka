Matryoshka - Linux Device Mapper
================================

dm-matryoshka is a Linux Device Mapper implementation for Matryoshka.

### Usage:

###### Dependencies:

* Linux kernel source
* Linux kernel headers
* gcc

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


## Contributing and License

Feel free to create Issues and issue Pull Requests/send Git patches to contribute to this project.

Code is dual-licensed under [GNU General Public License Version 2](LICENSE_GPL) and [BSD 3-Clause License](LICENSE_BSD).
