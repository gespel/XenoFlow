export DUMP_NAME="measurements/test5.pcap"
sudo tcpdump -i enp24s0f0np0 -e -nn -w $DUMP_NAME
