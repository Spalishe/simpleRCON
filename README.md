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
If you want to make input, then:
```bash
mkfifo rcon_input
cat > rcon_input | (cat > rcon_input & cat rcon_input) | unbuffer -p ./srcds -console | ./simpleRCON --conf rcon.conf --inpipe rcon_input
```
Note: your input would be doubled
## Client:
```bash
nc <ip> <port>
```

# Arguments:
```txt
--conf: Specifies path for configuration file
--inpipe: Specifies pipe at which server will write his STDIN
init: Creates basic configuration file
```

# Dependencies:
- JsonCpp
- Netcat (on client)

# License:
This project is licensed under MIT License. See [LICENSE](https://github.com/Spalishe/simpleRCON/blob/main/LICENSE)
