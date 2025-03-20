# CENG3020_Project

The goal of this project is to design a truly random and fair slot machine using the real-time operating system FreeRTOS. Check Lab_Project.pdf for further information.

# Building

To build this project you will need the following tools installed:

```
st-util
arm-none-eabi-gcc
```

Make sure to note where `arm-none-eabi-gcc` is installed and put it in the Makefile as the `TOOLCHAIN_ROOT`.

To write to the discovery board you need to run these commands in the project root:

```
make
make flash
```

Then push the reset (black) button on the discovery board. Sometimes the flash will say it failed, if it does just ignore it, it's probably fine...
