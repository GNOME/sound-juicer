# Sound Juicer

Copyright (C) 2003-6 Ross Burton

Licensed under the GPL, version 2 or greater.


This is Sound Juicer, a CD ripping tool using GTK+ and GStreamer.

Requirements:

- GNOME 3.22 Platform (GTK, gnome-media-profiles, etc.)
- GStreamer 1.0 and above

If the CD lookup is returning weird data, export `G_MESSAGES_DEBUG=sj-metadata`
before running sound-juicer to turn metadata debugging messages.


Common Problems
===============

### "Sound Juicer can't see my CD drive!"

Sound Juicer queries the kernel for CD drives, so you don't need to know the
device name.  For this to work you'll need some kernel modules loaded: either
ide-cd (for IDE drives) or sg (for SCSI drives).


### "Sound Juicer doesn't start, it can't find a plugin to access the CD drive!"

You need the GStreamer cdparanoia plugin.  Most distributions should ship this
with GStreamer, if you built it youself check you have the cdparanoia headers
installed.


### "I cannot encode to MP3!"

To encode to MP3 you need the GStreamer Lame plugin.  Most distributions don't
ship this due to patents and licence fees, so you will have to build lame
yourself, and then rebuild gstreamer-plugins.  Details of the pipeline
required is in the documentation.


### "It won't rip my CD!"

Try ripping the cd with cdparanoia, cdda2wav, grip, or any other CD ripper.  If
these work it is possible to rip the CD so file a bug with Sound Juicer.  If
none of these work, you've probably got a copy protected CD which your drive
won't read.  If it does work, try different output formats in Sound Juicer and
if some work while others don't, file a bug explaining what happens.
