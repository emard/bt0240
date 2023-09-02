# BT-0240 Configurator

Configurator for Bluetooth RS232 Adapter.
Device Original Manual with windows usage:
https://rainbow.com.ua/images/files/BT0240_User_Manual.pdf

Linux Usage:

Connect 7.5V power supply and turn the switch ON.
Green LED &#x1f7e9; should light up.

Find out MAC address

    # hcitool scan
    Scanning ...
           00:02:72:11:22:33      Adapter1

Push "CFG" button near power plug socket.
Yellow LED &#x1f7e7; should light up, indicating
configuration is allowed now.
Run configurator

    # ./bt0240 -d 00:02:72:11:22:33 -l
    Device name : Adapter1
    Encryption  : No
    Role        : Slave
    Discoverable: Yes
    Peer        : Any
    BPS Rate    : 9600
    Parity      : N
    Stopbits    : 1

bt0240 prints help

    # ./bt0240 -h
    Usage: bt0240 [OPTION]...
    Configurator for BT-0240 Bluetooth RS232C Adapter

      -h, --help              Print help and exit
      -V, --version           Print version and exit
      -d, --device=STRING     Serial Device or MAC of BT-0240
                                (default=`/dev/ttyS0')
      -b, --bps=INT           Serial Speed in bps [9600-1382400]
                                (default=`115200')
          --parity=STRING     Serial Parity [none|even|odd]  (default=`none')
          --stopbits=INT      Serial Number of Stop Bits  (default=`1')
          --role=STRING       Bluetooth Role [master|slave]  (default=`slave')
          --discoverable=INT  Bluetooth Device Discoverable (0|1)  (default=`1')
          --peer=STRING       Bluetooth Peer MAC [AA:BB:CC:DD:EE:FF|any]
                                (default=`any')
      -p, --pin=STRING        Bluetooth Security PIN [4-12 chars|off]
                                (default=`1234')
          --name=STRING       Bluetooth Device Name [1-31 chars]  (default=`Serial
                                Adapter')
          --reset             Configure Device to Default Settings  (default=off)
      -v, --verbose           Print extra info
      -l, --list              List Device Settings

Serial port can connect on-demand to the slave:

      # rfcomm bind 14 00:02:72:11:22:33
      # ls /dev/rfcomm14
      /dev/rfcomm14

Use the port. When connected, Blue LED &#x1F7E6; should turn ON and blink on traffic.

      # screen /dev/rfcomm14 9600

