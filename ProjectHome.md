The [Griffin Technologies](http://www.griffintechnology.com) iMate is a USB-ADB adaptor, which was [supported by Griffin up to OSX 10.3.x](http://www.griffintechnology.com/articles/168-is-the-imate-os-x-compatible).  Since then, despite working "plug and play" with mice and keyboards, its ADB controller functionality has been unsupported and unexposed.  This project provides a driver for OSX 10.4+, exposing that functionality, and allowing drivers to be written for more complex ADB devices, such as Wacom's range of discontinued and unsupported ADB graphics tablets.

The protocols for talking to the iMate were reverse-engineered using Apple's USB debugging tools, with some additional help from Joe Van Tunen, who had made a previous attempt at reverse-enginering Griffin's original OSX driver.  Griffin were open to providing help, but unfortunately had nobody available with the relevant technical knowledge.

Due to a failure to anticipate hot-pluggable ADB controllers in Apple's implementation of ADB under OSX, some code has been derived and / or duplicated from Apple's existing ADB driver code, and as such is licensed under the Apple Public Source License (APSL).  Please review this license at : http://www.opensource.apple.com/license/apsl/ before using any of these files.

Those running intel machines should see http://code.google.com/p/imate-osx/wiki/RunningOnIntel and either install the prebuilt IOADBFamily kernel extension or build and install it themselves before trying to install this driver.

## IMPORTANT NOTE ##

There have been some significant changes to the way driver code is written under Leopard and Snow Leopard.  I dread to think what's happened under Lion.  As a result, there are (to put it mildly), "issues" running this driver under Snow Leopard, and I haven't even tried under Lion.  Given that the majority of users will be looking at this in order to get their ADB Wacom tablets running, there is another solution :  Bernard from the bongofish.co.uk forums has taken a load of the info I've gleaned about how the ADB Wacoms work, and written an amazing piece of code for the "Teensy" embedded processor that allows (with a little soldering) use of the ADB tablets as USB devices using the newer Wacom drivers.  Basically, it's a "personality transplant" for the ADB tabs.

His code can be found here : http://code.google.com/p/waxbee/

To put it more bluntly : if you want to get your ADB (or serial) Wacom tablet working, you're best off shelling out a few ${LOCAL\_CURRENCY} for a Teensy (http://www.pjrc.com/teensy/) and using your tablet "native".