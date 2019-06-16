# attiny_daemon - Hardware and Software for a Raspberry UPS based on the Geekworm UPS Hat
If we are looking for a reliable UPS solution for a Raspberry Pi (e.g. as the backbone of our house automation) we can find a number of products. These are generally either badly maintained, or the are expensive, or they simply do not work. Let us look at a few examples.

### Pimodules
For instance, I own a UPS PICO made by [Pimodules](https://pimodules.com/), but it has problems with newer kernels, the developer/producer seems to have absolutely no inclination to help the customers even though over years he promised to provide new firmware version (and yes, I wrote numerous e-mails and got the promises also), and with the successor UPS PICO HV3.0A the same seems to happen. In the forum, the last post by anyone was mid-2018, and while the hardware specs of the system are fantastic, it is a closed-source system, so we can't even fix it afterwards. While it costs around 30€, the disadvantages are so grave that I would not suggest to buy one.

### S.USV
[Olmatic](https://olmatic.de/) produces UPSs that are actually quite good, they have a fantastic support and I'm currently using two of their S.USV pi basic for my home automation backbone consisting of two Raspberry Pis. The sad thing is that they do no longer produce this basic S.USV and you can't buy them any longer for any reasonable price (they cost me around 30€ including the battery). The successor costs roughly double that. For me a UPS that costs twice of the device that I want to protect doesn't make sense. One can rightly argue that this new version provides a lot of added functionality, but I do not need it for my use cases. So, if you have the spare money or need the additional functionality and do not want to build your own solution based on what is provided here, by all means, contact them and get one of their UPSs.

### Geekworm UPS Hat
An example for one UPSthat is very moderately priced and can be bought on AliExpress or Banggood (or even Ebay and Amazon) is the GeekWorm UPS Hat for the Raspberry Pi. At around 12€ it sounds fantastic, until you test it and realize that when power is cut and restored afterwards, the battery is no longer charged. You have to manually turn the system off and on again using a little button mounted on the side of the HAT. This absolutely disqualifies this as a UPS.

# The solution
### Make your Own
One solution would be to create our own solution, starting by designing the charging circuit for the battery, using a nice boost converter, adding an intelligent controller that can be programmed and re-programmed for different needs, create a schematic, PCB and let it be manufactured by one of the PCB manufacturers. I believe such a design could get to a price of around 25€, not counting the hours to be invested. I might, at some later point, come back to this approach.
### Use cheap components cobbled together
Actually, Peter Scargill started something like that in a [blog article](https://tech.scargill.net/more-uninterruptable-thoughts/) and the [follow-up](https://tech.scargill.net/the-kitchen-sink/), but he got sick and never picked up where he left. It seems that it worked pretty well, but has the single disadvantage that it is no HAT for the Raspberry and thus very far from a plug&play solution.
### Build on an existing solution
I bought the Geekworm UPS Hat over 2 years ago and became quite frustated with them. They went into my parts bin and waited for disassembly.

