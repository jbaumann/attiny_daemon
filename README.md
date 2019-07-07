# ATTiny Daemon - An affordable UPS for the Raspberry that actually works
Are you desperately searching for an UPS for Raspberry that doesn't cost twice as much as the actual device? Do you look for a UPS that simply works and is reliable? Do you look for simple extensibility and a hacker-friendly system that allows you to add your own functionality? Do you look for a project that is based solely on through-hole components (because not everyone is able to solder SMT components)?

Then search no further, you have come to the right place.

# Hardware and Software for a Raspberry UPS based on the Geekworm UPS Hat

This repository contains all the information for a tiny add-on PCB that turns the non-functioning Geekworm UPS HAT into a real UPS with a large number of additional features, including automatic shutdown and restart, temperature measurements of the battery, communication over I2C and an external button that can provide arbitrary functionality. The solution consists of three parts, the hardware based on an ATTiny45, the firmware for the ATTiny (currently at roughly 94% of the flash, so if you buy an ATTiny, go for the ATTiny85), and a systemd daemon written in Python with the accompying systemd unit file.

Oh, and if at any one time you ask yourself, "why the heck has he implemented this feature?", the answer is most probably "because we can..."

If you want to read about the details of hardware, firmware and software, jump to the [Wiki](https://github.com/jbaumann/attiny_daemon/wiki)

To whet your appetite here is a top view of the PCB designed for the ATTiny Daemon.

<img src="/hardware/Photo%20View%20Top.png" width="300">

# Introduction, or what happened so far...
If we are looking for a reliable UPS solution for a Raspberry Pi (e.g. as the backbone of our house automation) we can find a number of products. These are generally either badly maintained, or the are expensive, or they simply do not work. Let us look at a few examples.

### PiModules
For instance, I own a UPS PICO made by [Pimodules](https://pimodules.com/), but it has problems with newer kernels, the developer/producer seems to have absolutely no inclination to help the customers even though over years he promised to provide new firmware version (and yes, I wrote numerous e-mails and got the promises also), and with the successor UPS PICO HV3.0A the same seems to happen. In the forum, the last post by anyone was mid-2018, and while the hardware specs of the system are fantastic, it is a closed-source system, so we can't even fix it afterwards. While it costs around 30€, the disadvantages are so grave that I would not suggest to buy one.

### S.USV
[Olmatic](https://olmatic.de/) produces UPSs that are actually quite good, they have a fantastic support and I'm currently using two of their S.USV pi basic for my home automation backbone consisting of two Raspberry Pis. The sad thing is that they do no longer produce this basic S.USV and you can't buy them any longer for any reasonable price (they cost me around 30€ including the battery). The successor costs roughly double that. For me a UPS that costs twice the price of the device that I want to protect doesn't make sense. One can rightly argue that this new version provides a lot of added functionality, but I do not need it for my use cases. So I am in a problematic situation because I am not able to get a replacement unit if ever one of the UPSs dies without shelling out more money than I really want.
But if you have the spare money or need the additional functionality and do not want to build your own solution based on what is provided here, by all means, contact them and get one of their UPSs.

### Geekworm UPS Hat
An example for one UPS that is very moderately priced and can be bought on AliExpress or Banggood (or even Ebay and Amazon) is the GeekWorm UPS Hat for the Raspberry Pi. At around 12€ it sounds fantastic, until you test it and realize that when power is cut and restored afterwards, the battery is no longer charged. You have to manually turn the system off and on again using a little button mounted on the side of the HAT. This absolutely disqualifies this as a UPS.

### Others
Well, to cut a long story short, having evaluated a lot of different solutions, and to quote a very good song, even if totally out of context, "i still haven't found what i'm looking for"...

# Possible Solutions
### Make your Own
One solution would be to create our own solution, starting by designing the charging circuit for the battery, using a nice boost converter, adding an intelligent controller that can be programmed and re-programmed for different needs, create a schematic, PCB and let it be manufactured by one of the PCB manufacturers. I believe such a design could get to a price of around 25€, not counting the hours to be invested. I might, at some later point, come back to this approach.
### Use cheap components cobbled together
Actually, Peter Scargill started something like that in a [blog article](https://tech.scargill.net/more-uninterruptable-thoughts/) and the [follow-up](https://tech.scargill.net/the-kitchen-sink/), but he got sick and never picked up where he left. It seems that it worked pretty well, but has the single disadvantage that it is no HAT for the Raspberry and thus very far from a plug&play solution.
### Build on an existing solution
I bought the Geekworm UPS Hat over 2 years ago and became quite frustated with them because of having to turn them off and on manually. They went into my parts bin and waited for disassembly.
But recently I got into thinking that maybe I could use them as a basis for something that works exactly as I need it. I accidentally stumbled upon a [blog entry](https://tinkerman.cat/post/geekworm-power-pack-hat-hack) by Xose Pérez of Espurna Fame, and a [video](https://www.youtube.com/watch?v=7Vx_QIYrgQo) by Ralph S Bacon. Both used an ATTiny85 to enhance the functionality of a Geekworm UPS. Both used a simple solution to let the ATTiny do the switching and got a solution there, even though they do not address the charging problem.

The story could end here. But it didn't.

It somehow left me with a feeling of "this could be done better", and in best hacker-tradition I set out to do so.

# The final Solution
### The Approach
I started to reverse-engineer the Geekworm UPS hat, but then found the blog of BrouSant, a guy from the Netherlands who already had [analyzed the board in some detail and written about it](https://brousant.nl/jm3/elektronica/104-geekworm-ups-for-raspberry-pi). He also provides a [modification](https://brousant.nl/jm3/elektronica/105-geekworm-ups-for-raspberry-pi-simple-modification-detailed) by resoldering SMD resistors and adding tiny wires to make the UPS work as needed, and a more complete solution with a PIC microcontroller that controls everything. But to my eye it didn't look like the elegant solution I was looking for, and at the same time it needed the skills to solder SMD, which not everyone has (me included).

### Requirements - Wish List
Let us create our own list of requirements aka wish list:
- overall price of less than 20€ (including the HAT)
- soldering limited to through-hole components
- use of an ATTiny for its small form factor and simple programming
- simple build and simple modifiability of the programming on both ATTiny and Raspberry Pi
- minimum added physical height
- configuration of the ATTiny should be changeable from the Raspberry
- configuration of the ATTiny should be stored in the EEPROM
- watchdog functionality
- temperature measurement (rationalization: for monitoring the battery temperature)
- communication using I2C
- no blocking of Raspberry Pi pins (the hardware uses one I2C address, all the used pins are shared)
- minimal footprint of the additional hardware
- minimal modification of existing hardware
- daemon for the Raspberry written in Python for simplicity's sake
- everything should be configurable using a simple config file
- if no config file exists, it should be created with values on the ATTiny
- automatic sync of options in the config file between Raspberry and ATTiny
- automatic shutdown
- automatic restart
- different thresholds for warning, hard shutdown and restart
- external button to execute configurable functionality
- measurement of an additional external voltage (not sure why, but nice to have)
- configurable compensation for intrinsic measurement offsets and integral non-linearity
- minimal energy consumption

Incidentally, everything on this list is already implemented...

### The Implementation
We base our design on the Geekworm UPS HAT which we modify based on the idea of Brousant (see Wiki for details). We have to do this modification solely because the UPS doesn't continue charging the batteries after a power loss. If at any time the design is changed so that this functionality is provided then we can use an unmodified hardware. But since then we need the small modification.

On top of this we use an ATTiny with a minimum of additional components to implement one part of the functionality on our wishlist. A PCB designed for a minimal footprint while using only through-hole components can easily be connected to four pins of the Raspberry (SDA, SCL, 3.3V, GND) using a 2x2 Dupont connector.

On the Raspberry a daemon written in Python3 communicates with the ATTiny and complements its functionality to realize the full functionality we need.

The daemon reads a config file (per default in the same directory, configurable with a command line option), compares it with the ATTiny configuration, changes the ATTiny configuration if an option in the config file has a value different from that stored in the ATTiny, and adds non-existent configuration entries which have a value on the ATTiny to the daemon config. This leads to a very simple initial start with sensible values for most of the configuration options.

### The Files
Three sub-directories contain the necessary information:

- **hardware** - this directory contains Gerber files and board images. The board has been designed using [EasyEDA](http://easyeda.com) and if there is interest I can make the EasyEDA project public so you can simply order the board using their board manufacturing service [JLCPCB](https://jlcpcb.com/). It is important to know that even without the PCB i.e., building the hardware on a proto board is a perfectly valid approach and works like a charm (but still, a professional PCB is way cooler, right?).
- **firmware** - this directory contains the ATTiny implementation as an Arduino project. Simply open the project directory in your Arduino IDE, configure it for an ATTiny (45 or 85) and compile it. I personally program my ATTiny's with USBASP, an adapter which can be bought for small money.
- **daemon** - this directory contains the daemon, the unit file that allows us to install it as a service with systemd and an example configuration script. For first experiments, start the daemon with the option --nodaemon to allow for a graceful exit (i.e. no subsequent shutdown of the Raspberry Pi).

A fourth directory **miscelleaneous** contains additional pictures and diagrams used in the wiki pages.

And now it is time to head over to the [Wiki](https://github.com/jbaumann/attiny_daemon/wiki) to get details on how to install and modify the hardware and software, the detailed thoughts on the different parts of the implementations and tips and tricks for building the hardware and modifying the software. Have fun.
