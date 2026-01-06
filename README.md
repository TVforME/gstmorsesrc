# morsesrc
GStreamer plugin for converting morse code text to audio

## inspiration 💡🌟👩‍🎨
I needed a way to convert morse code yes:- dah dit dah dit dit dah dah into a sinewave representation to use in a GStreamer pipeline. One way is to generate using the online converters to wav file and simply use filesrc into the pipeline or alternatively use fdsrc or appsrc with both needing additional logic to implement.
filesrc with decodebin was far the better method however, required the first step to generate a wave file for each morse message. 

I thought 🤔 Why not try to convert the text string directly like into dah's and dit's with GStreamer!

This is my first attempt with building a gstreamer plugin.

I enjoy playing with GStreamer as do similar minded "amateur radio" aficionados. I hope others find my plugin useful in their projects.

## Features

- Converts text input to Morse code audio.
- Adjustable parameters for
  1. Morse speed in WPM,
  2. Frequency in Hz,
  3. Volume, 0.0 - 1.0,
  4. One-shot. true/false.

 ### Emit Bus message
  - "about-to-finish" message to gstreamer bus to notify 90% before buffer end.
  - "morse-playback-complete" message to gstreamer bus on playback completion.

## Introduction
The morsesrc uses a lookup table method in a compact and efficient way storing the Morse code sequences for all the ASCII characters by mapping each ASCII character to their corresponding Morse code sequences.
Each char in the text string is 'looked-up' and converted to its corresponding dit or dah followed by a space OR a longer space between words.
The dit's, dah's and spaces are converted to audio sinewave and pushed out of the plugin at the selected samplerate and indianess.
Once the text string reaches the end the plugin signals a EOS and cleans up.

## Usage

```bash
gst-launch-1.0 morsesrc text="CQ CQ DE VK3DG" wpm=20 frequency=880.0 volume=0.5 ! audioconvert ! autoaudiosink
```

## Requirements

- GStreamer 1.0 or later
- GStreamer Plugins Base 1.0 or later
- Glib 2.0 or later
  
## Dependencies

Ensure that GStreamer and its development libraries are installed:

```bash
sudo apt-get update
sudo apt-get install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

## Installation build tools

```bash
sudo apt-get install python3-pip
pip3 install meson ninja
```
## Building -> using meson

```bash
# clone ftom repo to local machine
git clone https://github.com/TVforME/morsesrc
cd morsesrc

// Adjust --libdir to point to your GStreamer plugin directory..
// Local directory --libdir=/usr/local/x86_64-linux-gnu/gstreamer-1.0
// I perfer to install in system gstreamer directory therefore use --libdir=/usr/lib/x86_64-linux-gnu/gstreamer-1.0

meson setup builddir --libdir=/usr/lib/x86_64-linux-gnu/gstreamer-1.0 
meson compile -C builddir
sudo meson install -C builddir

//Check plugin is built and installed. You should see libmorsesrc.so is listed.
ls /usr/lib/x86_64-linux-gnu/gstreamer-1.0/

//Check plugin is built and installed. You should see libmorsesrc.so is listed.
ls /usr/lib/x86_64-linux-gnu/gstreamer-1.0/

// Check gst-inspect-1.0 is able to list plugin features.
gst-inspect-1.0 morsesrc

// Test its working with gst-launch-1.0
gst-launch-1.0 morsesrc text="CQ CQ DE VK3RGL" ! autoaudiosink

```
## Building -> using make

How to use it
Build (as normal user):
code
```bash
# clone from repo to local machine
git clone https://github.com/TVforME/morsesrc
cd morsesrc
make
```
Install (as root):
You must use sudo because you are writing to the system /usr/lib folders.
```bash
# Install to system
sudo make install

# View build configuration
make info

# Clean and rebuild
make rebuild

# Uninstall
sudo make uninstall

# Verify installation
make verify

gst-launch-1.0 morsesrc text="CQ CQ DE VK3EHT" wpm=20 frequency=880.0 volume=0.5 ! audioconvert ! autoaudiosink

```
## License
This project is licensed under the GNU Lesser General Public License v3.0 - see the [LICENSE](LICENSE) file for details.
