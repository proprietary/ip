# a "What is my IP?" server

Linux only because this uses `epoll(7)`

## Building

```bash
bazel //:ip
./bazel-bin/ip --port=50888
```

## Example usage

```bash
# get my IPv6
curl -6 <deployed host>:50888

# get my IPv4
curl -4 <deployed host>:50888
```

## License

Apache-2.0
