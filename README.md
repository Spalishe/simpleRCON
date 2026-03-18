Simple RCON program. Literally simple.

# Building:
```bash
git clone https://github.com/Spalishe/simpleRCON
cd simpleRCON
make
```

# Using:
## Server:
```bash
unbuffer -p ./srcds -console | ./simpleRCON --conf rcon.conf
```
## Client:
```bash
nc <ip> <port>
```

# Arguments:
```txt
--conf: Specifies path for configuration file
init: Creates basic configuration file
```

# Dependencies:
- JsonCpp
- Netcat (on client)

# License:
This project is licensed under MIT License. See [LICENSE](https://github.com/Spalishe/simpleRCON/blob/main/LICENSE)
