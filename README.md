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

Monitor (ie. connect to serial):

```
pio device monitor
```

List available devices:
```
pio device list
```
```
