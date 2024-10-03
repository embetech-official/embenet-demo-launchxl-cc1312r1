# embeNET Demo for LAUNCHXL-CC1312R1 board

## What's inside ?

This repository contains an exemplary project demonstrating the use of embeNET Suite on [LAUNCHXL-CC1312R1](https://www.ti.com/tool/LAUNCHXL-CC1312R1) evaluation boards. 
The project is designed for the [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO). 

## What's included ?

The demo includes the following components of the embeNET Suite:
- The embeNET Node library in demo mode, providing IPv6 wireless mesh networking connectivity
- A port of the embeNET Node for the CC1312R1 chip
- embeNET Node Management Service (ENMS) providing telemetry services
- MQTT-SN client

## What are the demo version limitations ?

The demo *can only be used for evaluation purposes* (see LICENSE.txt for details).
The demo is limited to 10 nodes only (including root node).

## What you'll need to run the demo

- PC with Windows
- One [NUCLEO-WL55JC](https://www.st.com/en/evaluation-tools/nucleo-wl55jc.html) board connected to the PC (via USB cable) that will act as the root of the network
- At least one more (and up to 9) [LAUNCHXL-CC1312R1](https://www.ti.com/tool/LAUNCHXL-CC1312R1) boards that will act as the network nodes
- Code Composer Studio, available for download from [the official Texas Instrumets site](https://www.ti.com/tool/CCSTUDIO)
- [embeNET demo package for LAUNCHXL-CC1312R1](https://github.com/embetech-official/embenet-demo-launchxl-cc1312r1/releases)

Optionally, to play with the MQTT-SN demo service you'll need:
- MQTT For Small Things (SN) written in Java, available for download from [github](https://github.com/simon622/mqtt-sn)
- MQTT Client Toolbox, available for download from [the official MQTTX page](https://mqttx.app)

Optionally, to easily interact with the custom UDP service you'll need
- [UDP - Sender/Reciever app from Microsoft Store](https://www.microsoft.com/store/apps/9nblggh52bt0)

## How to start?

Read the ['Getting started with embeNET demo for CC1312 using LAUNCHXL-CC1312R1 board'](https://embe.tech/docs/?q=doxyview/Getting%20started%20with%20LAUNCHXL-CC1312R1/index.html) tutorial.