#!/bin/sh

#GPIOA4 = 480+4 = 484
echo 484 |  tee /sys/class/gpio/export 
echo out |  tee /sys/class/gpio/gpio484/direction 
echo 0 |  tee /sys/class/gpio/gpio484/value 
sleep 0.1 
echo 1 |  tee /sys/class/gpio/gpio484/value