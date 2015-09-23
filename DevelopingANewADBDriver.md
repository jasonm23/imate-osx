# Introduction #

So, you have an ADB Device, and you want to develop a driver for it under OSX.  There's 3 questions you need to ask yourself before you start:

  1. Has someone else already done it?
  1. Are you sure you want to do this?
  1. Are you completely mental?

Really, it's not easy, but here's a good set of starting rules.

## What you need ##

  * Your device
  * A driver for your device running under "classic" MacOS
  * A couple of older macs, at least one of which is equipped with an ADB port, and the other of which is equipped with a serial port, both of which are running "classic" MacOS somewhere between 7.5.x and 9.x
  * A serial cable and an ADB (or s-video) cable which can be sacrificed (you'll be cutting both of them in half)
  * 1 x 2K and 1 x 10K resistor
  * A copy of (at least) [ADB Analyzer](ftp://ftp.apple.com/developer/Tool_Chest/Devices_-_Hardware/Apple_Desktop_Bus/ADB_Analyzer_1.0D6.sit.hqx)
  * An idea of what the ADB protocol looks and feels like, you'll want to read at least the first few parts of [Inside Macintosh - ADB Manager](http://developer.apple.com/documentation/Mac/Devices/Devices-203.html)
  * A decent grasp of IOKit device drivers, and probably an idea of what HID devices are all about

## Getting your shit together ##

First, you'll want to make up your protocol sniffing cable.  The schema is in the ADB Analyzer application itsef, but I've reproduced it here :

![http://imate-osx.googlecode.com/files/cable.png](http://imate-osx.googlecode.com/files/cable.png)

The "ADB" end plugs into the adb bus of the "host" machine, where you'll be running the relevant driver and also plugging in your device.  The other end goes to the serial port of the other machine, which will be running the analyzer application.

Fire up both machines, with the cable between them, your device plugged into the bus of the host machine, and a driver up and running.  No magic smoke let out of the machines?  Good.

## Protocol Analysis ##

Now, you want to see what happens when the ADB bus is probed, what the driver does to make your device do its thing.  Use something to reinitialize the bus on the "host" machine(ADB Reinit or ADB Parser should do the trick, both of these are available from the same place as ADB Analyzer) while ADB Analyzer is waiting for input.  You may need to do this a lot to work out  what packets are being sent.  No, ADB analyzer doesn't let you save dumps.  Nor does it collect very much before stopping (about 3 seconds worth maximum).  You'll need a pen and paper, or another machine, to note the various packets that are being sent.

Usually, you'll see the driver trying to set the handler ID of your device to something other than its default.  You need to know what this is, because it's the "knock on the door" that the device will need to give up all its secrets to you in your OSX driver.

Next up, capture packets from using the device, work out what packets are sent backwards and forwards when you do what.  Think hard.  Think harder. Reverse-engineer the protocol.  Look at what happens if you mess with any related control panels.  Reverse engineer that as well, if needs be.

Right, you're done with "classic" MacOS.  Relegate the machines back to the dusty cupboard of discontinued tech, and get on with writing a driver.  Fun fun.

## The driver ##

Probably the best way to write your driver is to make it convert the ADB protocol you just deciphered to HID, that way you have a running chance of getting your device recognised by the rest of the system.

If you decide to go this route, there is a separate project in the svn repository for ADBHIDDevice, which does most of the donkey work for you.  It also contains a sample driver for your bog-stock ADB mouse.

So, make yourself a new IOKit Driver project, and make your new class, with a superclass of ADBHIDDevice.

### Getting it to match your device, and other Info.plist guff ###

Matching is a piece of cake.  You'll want to make a dictionary entry in the IOKitPersonalities array of info.plist, and it should have the following settings

```
IOClass : YourClassName
IOProviderClass : IOADBDevice
ADB Match : 0xAA-0xHH
```

The AA and HH in the ADB Match line should be replaced by the default Address and Handler that your device identifies itself as.  'YourClassName' should obviously be replaced by your driver Class' name

That much is enough that, when an ADB device with the right address and handler crops up, your driver will get a shot at handling it.  Neato, right?  Yep, but there's more.  Although your class is a (fake) HID device, it won't get hooked into the HID system automatically.  That's a bit arse, and we need a bit of voodoo to get it to work.  We'll come back to this later.

### The Driver ###

There's a load of stuff in the example mouse driver you can pretty much copy across, which should make your life easier.  You'll want vendor and product IDs from somewhere, these should not collide with existing USB vendor and product IDs unless you're trying to do something really gnarly like pretending your ABD device is an existing USB HID device...

Next up, you're going to need a HID descriptor.  There's nothing nice about this, it's a horrendous job and about the oly HID descriptor editing tool I know of runs under windows.  Woopla.  So, make up your HID descriptor, export it to an ".h" file somewhere, and (I would suggest) bring it into your class in the same way I have done for the mouse driver (as a static class member)

Make up your hid report typedef at the same time.

Now make up a method that populates your HID report(s) and sends them off using `handleReport()`.  Your work is nearly done.

### Back to Info.plist, or _voodoo time_ ###

Now, as I said earlier, you won't be fully hooked up to the HID system if you try running your driver now.  You'll handle your packets and all that jive, but your driver won't have an IOHIDEventDriver hanging onto it.  For that, we need to go back to Info.plist, and add a dummy entry to make that happen.  Looking at the example mosue driver, this is the "HID System Hookup" in the personalities list.

So : add another personality to Info.plist, and add the following:

```
CFBundleIdentifier: com.apple.kernel.iokit
IOClass: IOHIDEventDriver
IOProviderClass: IOHIDInterface
VendorID: VVVV
ProductID: PPPP
```
What this does is tell IOKit that when it gets an IOHIDInterface with the given vendor and product ID, it should fire up all the rest.  Easy enough, and you get the IOHIDInterface by default, so all should now be well.  Obviously, VVVV and PPPP are the vendor (and optionally product) ids that you are returning from your code.

And that's it.

Cake!

