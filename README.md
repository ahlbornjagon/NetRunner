# NetRunner:

## Building

- Building ESP32 code:
    - Go to ~/.espressif folder, run the `./install.sh` script, then `source export.sh`
    - Go to root NetRunner folder and run `idf.py build`
    - Flash with `idf.py flash`
- Building receiver program:
    - Go to /main and run `make rec`

## Running
- Terminal one:
    - Wanna see the esp32 output? Run `idf.py monitor`
- Terminal two:
    - run `./rec -p <port> -b <baud>` Or `./rec` if you wanna use default port `/dev/ttyUSB1` and default baud `115200`


## WIP: This will be a tool used to scan local AP's and display various information about them, then use an algorithm to provide a score of its security rating. The aim of this tool is to spot evil twin networks eventually.

## Scope
1. Via UART communicate with the esp32 and get all the info via a protobuf packet
2. Parse protobuf packet, print to console (just to start)
3. Eventually, host webpage and display there

## Future Scope
- Remove the UART interface (just doing this for debug) and just host webpage.
- Also include a sniffer and display packet metrics on webpage