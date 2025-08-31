#!/usr/bin/env -S bash -e

if [ -z "west list -f '{name}' | grep ^zephyr$" ]; then
    ZEPHYR_DIR=$(west list -f '{abspath}' zephyr)
else
    # Assume zephyr is the manifest repo
    ZEPHYR_DIR=$(west list -f '{abspath}' manifest)
fi

CANNECTIVITY_SCRIPTS_DIR=$(dirname $(realpath $0))
NET_TOOLS_DIR=$(west list -f '{abspath}' net-tools)

echo "Zephyr:    $ZEPHYR_DIR"
echo "net-tools: $NET_TOOLS_DIR"

cleanup() {
    echo "Detaching USB/IP"
    sudo usbip detach -p 0 || true

    if [ -n "$CANNECTIVITY_PID" ]; then
        echo "Stopping CANnectivity"
        kill -SIGINT $CANNECTIVITY_PID || true
    fi

    if [ -n "$CANDUMP_PID" ]; then
        echo "Stopping candump"
        kill -SIGINT $CANDUMP_PID || true
    fi

    for i in `seq 0 2`; do
        vcan="cannectivity$i"
        echo "Bringing down virtual CAN interface $vcan"
        sudo $NET_TOOLS_DIR/can-setup.sh --iface $vcan down || true
    done

    echo "Bringing down network"
    sudo $NET_TOOLS_DIR/net-setup.sh down || true
}

trap cleanup EXIT SIGINT

echo "Bringing up network for USB/IP"
# TODO: fix net-setup.sh up returning error 2
sudo $NET_TOOLS_DIR/net-setup.sh up || true

for i in `seq 0 2`; do
    vcan="cannectivity$i"
    echo "Bringing up virtual CAN interface $vcan"
    sudo $NET_TOOLS_DIR/can-setup.sh --iface $vcan up
done

echo "Starting candump"
candump -t a -H any > host-tests-candump.log 2>&1 &
CANDUMP_PID=$!

echo "Building CANnectivity"
west build -b native_sim/native/64 -S usbip-native-sim app -- -DFILE_SUFFIX=usbd_next

echo "Starting CANnectivity"
valgrind --suppressions=$ZEPHYR_DIR/scripts/valgrind.supp --leak-check=full \
         build/zephyr/zephyr.exe > host-tests-cannectivity.log 2>&1 &
CANNECTIVITY_PID=$!

echo "Waiting for CANnectivity to start..."
sleep 10

echo "Attaching USB/IP"
sudo modprobe vhci_hcd
sudo usbip attach -r 192.0.2.1 -b 1-1

echo "Waiting for USB/IP to attach..."
sleep 10

for i in `seq 0 2`; do
    iface="can$i"
    echo "Configuring CANnectivity CAN interface $iface"
    sudo ip link set $iface type can bitrate 125000 fd on dbitrate 1000000
    sudo ip link set $iface up
done

export CAN_INTERFACE=socketcan
export CAN_CONFIG='{"fd": true}'

for i in `seq 0 2`; do
    iface="can$i"
    vcan="cannectivity$i"
    echo "Testing CANnectivity CAN interfaces $iface <-> $vcan"
    export CAN_CHANNEL=$iface
    sudo ip link property add dev $vcan altname zcan0
    west twister -T $ZEPHYR_DIR/tests/drivers/can/host/ -v --platform native_sim/native/64 -X can
    sudo ip link property del dev $vcan altname zcan0
done
