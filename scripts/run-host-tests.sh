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

    echo "Bringing down virtual CAN busses"
    sudo $NET_TOOLS_DIR/can-setup.sh --config $CANNECTIVITY_SCRIPTS_DIR/cannectivity.conf down \
        || true

    echo "Bringing down network"
    sudo $NET_TOOLS_DIR/net-setup.sh down || true
}

trap cleanup EXIT SIGINT

echo "Bringing up network for USB/IP"
# TODO: fix net-setup.sh up returning error 2
sudo $NET_TOOLS_DIR/net-setup.sh up || true

echo "Bringing up virtual CAN busses"
sudo $NET_TOOLS_DIR/can-setup.sh --config $CANNECTIVITY_SCRIPTS_DIR/cannectivity.conf up

echo "Starting candump"
candump -t a -H any > host-tests-candump.log 2>&1 &
CANDUMP_PID=$!

echo "Building CANnectivity"
west build -b native_sim/native/64 -S usbip-native-sim app -- -DFILE_SUFFIX=usbd_next

echo "Starting CANnectivity"
./build/zephyr/zephyr.exe > host-tests-cannectivity.log 2>&1 &
CANNECTIVITY_PID=$!

echo "Waiting for CANnectivity to start..."
sleep 10

echo "Attaching USB/IP"
sudo modprobe vhci_hcd
sudo usbip attach -r 192.0.2.1 -b 1-1

echo "Waiting for USB/IP to attach..."
sleep 10

for i in `seq 0 2`; do
    echo "Configuring CANnectivity CAN interface can$i"
    sudo ip link set can$i type can bitrate 125000 fd on dbitrate 1000000
    sudo ip link set can$i up
done

export CAN_INTERFACE=socketcan
export CAN_CONFIG='{"fd": true}'

for i in `seq 0 2`; do
    echo "Testing CANnectivity CAN interface can$i"
    export CAN_CHANNEL=can$i
    sudo ip link property add dev cannectivity$i altname zcan0
    west twister -T $ZEPHYR_DIR/tests/drivers/can/host/ -v --platform native_sim/native/64 -X can
    sudo ip link property del dev cannectivity$i altname zcan0
done
