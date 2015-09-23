# Running on Intel-based macs #

Under Leopard (at least) on intel-based machines, Apple's base ADB controller kernel extension, IOADBFamily.kext, is not installed.  This is a problem, as it contains the superclasses and user client classes for required by the iMate driver.  In order to build and / or run the iMate driver, it is necessary to build or install the Apple driver as well.

A pre built universal binary version of this extension for OSX 10.4 and above is available from : http://imate-osx.googlecode.com/files/IOADBFamily-6.dmg.gz

<font color='red'><b>If you already have a copy of IOADBFamily.kext in /System/Library/Extensions DO NOT INSTALL THIS</b></font>

## Details ##

### Obtaining the source ###

The source for IOADBFamily can be downloaded from Apple's public source site.  The required file is http://opensource.apple.com/tarballs/IOADBFamily/IOADBFamily-6.tar.gz

### Building the kernel extension ###

In order to build the extension, you will need Apple's developer tools installed on your machine.  These should have come with your copy of OSX, or can be downloaded at http://connect.apple.com (Apple Developer Connection membership required, the 'online' or, in other words, free, membership is sufficient)

That's all well and good, but unfortunately you'll notice that there is not an xcode project in the folder, only a .pbproj file suitable for Project Builder (XCode's predecessor) or XCode 2.x.  There's an additional gotcha, in that one of the defnined constants used by the project is undefined on intel.  Fear not, though, for we have a fix for both of these issues:

Open a terminal window, change directory to the project root directory (e.g. ~/Desktop/IOADBFamily-6), and issue the following commands:

```
svn co http://imate-osx.googlecode.com/svn/trunk/IOADBFamily-6 .
xcodebuild -configuration Release DSTROOT=/ clean build 
sudo xcodebuild -configuration Release DSTROOT=/ install
```

Alternatively, download the pre-built version here : http://imate-osx.googlecode.com/files/IOADBFamily-6.dmg.gz