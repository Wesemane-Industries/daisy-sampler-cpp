# daisy-sampler-cpp

WIP: audio sampler using daisy seed

## Dependencies

```bash
sudo apt update && sudo apt install build-essential -y
```

now create dir and get compiler and Examples

```bash
mkdir daisy && cd daisy

# get arm gcc, note it has to be v10.3-2021-10
# for linux x86_64
curl -L -O "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2?rev=78196d3461ba4c9089a67b5f33edf82a&hash=D484B37FF37D6FC3597EBE2877FB666A41D5253B"

# for macOS
curl -L -O "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-mac.tar.bz2?rev=58ed196feb7b4ada8288ea521fa87ad5&hash=62C9BE56E5F15D7C2D98F48BFCF2E839D7933597"

# unpack
tar -xvf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
# you should now have a gcc-arm-none-eabi-10.3-2021.10 dir

# delete tar bz2
rm gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2

# now clone DaisyExamples currently you have to apply this PR https://github.com/electro-smith/libDaisy/pull/663
# to make the wav writer work properly
git clone --recurse-submodules https://github.com/electro-smith/DaisyExamples ~/Desktop/DaisyExamples
```

## Flash

```bash
export GCC_PATH=/path/to/gcc-arm-none-eabi-10.3-2021.10/bin && export PATH=$GCC_PATH:$PATH
# if you are using stlink-v3 debug probe
make clean; make; make program
# or for debug 
make clean; DEBUG=1 make; make program

# if you are not using stlink-v3 debug probe, this guide explains flashing https://daisy.audio/tutorials/cpp-dev-env/
```

### Logging

If you are using ST-Link v3 and you uncommencted the `LOGG` define in [./src/main.cpp](./src/main.cpp)
there will be two devices one for the actual microcontroller and one for the
debug probe, the one for the debug probe only exists if a logger is initialized

```bash
picocom -b 115200 /dev/ttyACM* 
```

Caution picocom uses `CTRL` + `a` keybindings this might collide with tmux.

## Remote flashing

If you daisy is connected to e.g. a raspi on your network you can flash it by doing

```bash
make clean; DEBUG=1 make  # remove DEBUG for release build
scp -r build pi@raspberrypi.local:/home/pi/workspace/daisy-seed-cpp/flash  # make sure the destination exists
```

then on your raspi (assuming you are using STLINKv3 connected to it and daisy seed connected to it)

```bash
ssh pi@raspberrypi.local
cd /home/pi/workspace/daisy-seed-cpp/flash  # this is the same as above yours might differ

# now flash please check if your openocd installation is in /usr/share/openocd/scripts
#  or /usr/share/local/openocd/scripts
openocd -s /usr/share/openocd/scripts -f interface/stlink.cfg -f target/stm32h7x.cfg \
                -c "program ./build/daisy-sampler-cpp.elf verify reset exit"
```

Now if you did the DEBUG build and want to see the logs you can

```bash
picocom -b 115200 /dev/ttyACM*
```

where * is 0/1/2 depending on what you have connected, make sure you connect to the
daisy seed, semihosted logging through STLINKv3 is not supported yet
