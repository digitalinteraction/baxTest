# BAXTest

A command line utility originally written for testing BuildAX LRS devices. 

Allows the decoding of encrypted BAX data files using a `BAX_INFO.BIN` file, 
and communicating directly with devices using the Si44 radio (firmware required)


## Setup

Running this software requires a number of things:

 * A radio-only firmware flashed to a BAX LRS or BAX USB
 * A radio setup script / config file (BAX_SETUP.CFG) (example [here](https://gist.github.com/sjmf/ba762f745bcccd28db1c1d22d2b281ee))
 * A binary of this software (compile by typing `make`)
 * Some BAX sensors (obviously)


## Example Command

```
./BAXTest -sS -fE -eH -dCOM3 -oF -tDAT00000.BIN -pPNDE -rIPF -iBAX_INFO.BIN -cBAX_SETUP.CFG
```

Explanation:

 * *-sS*             - Source: Serial Port
 * *-fE*             - Talk to the firmware in radio event format
 * *-eH*             - Expect output from firmware in Hexdecimal
 * *-dCOM3*          - Read from serial port `COM3`. On linux this will usually be `/dev/ttyACM0`, or `/dev/cu.usbserial` on OSX
 * *-oF*             - Output to a file, rather than `stdout`
 * *-tDAT00000.BIN*  - The file to write to.
 * *-pPNDE*          - Filter to exclude packet types not matching these (default, see below)
 * *-rIPF*           - Load, pair and add sensors (default, see below)
 * *-iBAX_INFO.BIN*  - Load and save pairing data from/to this file
 * *-cBAX_SETUP.CFG* - Load radio settings from this file


## Help page

Running the utility without command options will produce the following help page:

```
Input options:
    'S'ource:       Default: Serial port
                    SerialPort      'S'
                    File            'F'
                    UDP             'U'

    'F'ormat        Default: Radio Events
                    Radio events    'E'
                    Binary units    'U'

    'E'ncoding      Default: Hex ascii
                    Raw binary      'R'
                    Hex ascii       'H'
                    Slip encoded    'S'

    'D'escriptor,   Default: COM1
    (COM1 , DAT12345.BIN, 192.168.0.100+12-34-56-78-9A-BC+username+password)

Output options:
    'O'utput        Default: stdout
                    File            'F'
                    Stdout          'S'

    Output 'M'mode  Default: Hex ascii
                    Raw binary      'R'
                    Hex ascii       'H'
                    Slip encoded    'S'
                    CSV output      'C'

    Outpu'T' file   Default: output.out
                    e.g. output.bin

Bax settings:
    'P'acket filtering    Default: PNDE (all)
                    Pairing packets 'P'
                    Name packets    'N'
                    Decrypted       'D'
                    Encrypted       'E'
                    Encrypted       'R'

    'R'adio decryption settings    Default: IPF
                    Load info file   'I'
                    Pair new devices 'P'
                    Add to info file 'F'

    'I'nfo file name Default: BAX_INFO.BIN
                    e.g. BAX_INFO.BIN

    'C'onfig file name Default: BAX_SETUP.CFG
                    e.g. BAX_SETUP.CFG

Press any key to exit....

```

## Licence

Copyright (c) 2013-2014, Newcastle University, UK. All rights reserved.

