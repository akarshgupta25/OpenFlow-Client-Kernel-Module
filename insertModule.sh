#! /bin/bash

if [ "$1" == "" ]; then
    echo "Please provide the controller IP address as argument to the script"
    exit
fi

pingResults=`ping -c 1 $1 | grep "ttl=64" 2>/dev/null`
if [ "$pingResults" == "" ]; then 
    echo "Unable to ping the controller. Exiting now."
    exit
fi

cntrlInt=$1
interfaces=""
for inter in `ifconfig -a | grep "Link encap" | awk '{print $1}'`
do
    if [ "$inter" != "lo" ] && [ "$inter" != "eth0" ]; then
        out=`ifconfig -a | grep $inter -A 1 | grep "inet addr" | awk '{print $2}' | grep -o "[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*"`
        if [ "$out" == "" ]; then
            if [ "$interfaces" != "" ]; then
                interfaces="$interfaces,\"$inter\""
            else
                interfaces="\"$inter\""
            fi
            ifconfig $inter up
        else
        fi
    fi
done

# echo "Interfaces are $interfaces test"
# echo "controller IP is $cntrlInt test"
if [ "$2" == "" ];then
    sudo insmod ./openflowclient.ko gpOpenFlowIf=$interfaces gpServerIpAddr=$cntrlInt 
else
    sudo insmod ./openflowclient.ko gpOpenFlowIf=$interfaces gpServerIpAddr=$cntrlInt gCntrlPortNo=$2
fi
