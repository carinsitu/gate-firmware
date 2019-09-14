# Gate Node

## Wiring

GPIO 4 (D2) is connected to IR sensor

## Development environment

### Setup platformio

```shell
pip install --user virtualenv
```

Add in `~/.profile`:

```shell
if [ -d "$HOME/.local/bin" ] ; then
    PATH="$HOME/.local/bin:$PATH"
fi

if [ -d "$HOME/.platformio/penv/bin" ] ; then
    PATH="$HOME/.platformio/penv/bin:$PATH"
fi
```

Reload `~/.profile`:

```shell
source ~/.profile
```

```shell
virtualenv $HOME/.platformio/penv
```

### Usage

 * Compile

     ```
     pio run
     ```

 * Compile and upload through serial

    ```
    pio run -t upload
    ```

 * Compile and upload through OTA

    ```
    avahi-browse -a -t # List available mDNS services
    avahi-browse -t _arduino._tcp | grep GateNode | awk -F' ' '{ print $4 }' # List gate nodes mDNS hostnames
    pio run --target upload --upload-port TARGET_FQDN.local
    ```

    If you want a massive OTA update:

    ```
    for i in `avahi-browse -t _arduino._tcp | grep GateNode | awk -F' ' '{ print $4 }'`; do
      pio run --target upload --upload-port ${i}.local
    done
    ```

 * Monitor (ie. connect to serial)

    ```
    pio device monitor
    ```

 * List available devices

    ```
    pio device list
    ```
