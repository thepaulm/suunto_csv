# suunto_csv

Would you like to pull dive logs off of your Suunto dive computer watch without installing hundreds of megs of mono framework?
Me too. This suunto_csv was made from libdivecomputer.

Suunto dive computer watches use serial over USB - so first install an osx ftdi driver, for example:

https://cdn.sparkfun.com/assets/learn_tutorials/7/4/FTDIUSBSerialDriver_v2_3.dmg


Then just run suunto_csv.

Example use:
```
$ ./suunto_csv D4i
Got dive: 1-2-2017 11:16:35
Got dive: 1-2-2017 9:39:34
Got dive: 3-6-2015 12:5:47
Got dive: 3-6-2015 10:24:28
```

Each of those files is a csv with columns {time, depth}.
