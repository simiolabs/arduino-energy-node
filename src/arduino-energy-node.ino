/*
Simio Labs's energy monitor application. This program reads current
and voltage from 6 sensors, calculates power consumption and sends
the data to a coordinator using Zigbee technology.

Copyright (C) 2013 Simio Labs, created by Daniel Montero
Copyright (C) 2017 Simio Labs, modified by Daniel Montero

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <EmonLib.h>
#include <XBee.h>

EnergyMonitor emon;
XBee xbee = XBee();

uint8_t payload[42] = { 0 }; // XBee payload

// SH + SL Address of receiving XBee
XBeeAddress64 logger64 = XBeeAddress64(0x0013a200, 0x4092d77c);
ZBTxRequest loggerZbTx = ZBTxRequest(logger64, payload, sizeof(payload));
ZBTxStatusResponse loggerTxStatus = ZBTxStatusResponse();

// change node's number
char node = '0';

// change number of current sensor used (1 - 6)
const int sensors = 1;

// change voltage calibration number:
// 91.66 for 120VAC-12VAC, 183.33 for 240VAC-12VAC
const float calib120 = 91.66;
const float calib240 = 183.33;

// change it to specify voltage sensor pin
//                    S0  S1  S2  S3  S4  S5
int voltSensor[6] = { 2 , 8 , 8 , 8 , 8 , 8 };
float calib[6] = { calib240, calib240, calib240, calib240, calib240, calib240 };
const float phaseShift = 1.7;

const float currentCalib = 16.66;

const float wavelengths = 20;
const float timeout = 2000;

long interval = 6000; // interval at which to do something (milliseconds)

float lastPower = 0;
float presentPower = 0;

const int dataLength = 8; // length of data
const int dataNum = 5; // number of data inputs

void setup() {
  Serial.begin(115200); // start serial port
  xbee.begin(Serial); // initialize Xbee module
}

void loop() {
  char data[dataLength] = { 0 };
  float dataArray[dataNum] = { 0 };

  for (int i = 0; i < sensors; i++) {
    // configure sensors according to setup (above), don't change here
    emon.voltage(voltSensor[i], calib[i], phaseShift); // voltage: input pin, calibration, phase shift
    emon.current(i, currentCalib); // current: input pin, calibration

    // calculate values
    emon.calcVI(wavelengths, timeout); // no. of wavelengths, time-out

    // get values from sensors
    dataArray[0] = emon.realPower;
    dataArray[1] = emon.apparentPower;
    dataArray[2] = emon.Irms;
    dataArray[3] = emon.Vrms;

    // calculate energy
    presentPower = dataArray[0];
    dataArray[4] = ((presentPower + lastPower) / 2) * interval / 3600000;
    lastPower = presentPower;

    // print out the results
    Serial.print("N");
    Serial.print(node);
    Serial.print(",");
    Serial.print("I");
    Serial.print(i);
    Serial.print(",");
    Serial.print("V");
    Serial.print(voltSensor[i]);
    Serial.print(": ");
    emon.serialprint(); // print out all variables
    Serial.print("E: ");
    Serial.println(dataArray[4]);

    // convert floats to strings and add them to the payload
    for (int i = 0; i < 5; i++) {
      // dtostrf(floatVar, minStringWidthIncDecimalPoint, numVarsAfterDecimal, charBuf)
      dtostrf(dataArray[i], 1, 2, data);

      switch (i) {
        case 0: // real power
          add2payload(data, 0, 8); // data, position, length
          break;
        case 1: // apparent power
          add2payload(data, 8, 8);
          break;
        case 2: // current
          add2payload(data, 16, 8);
          break;
        case 3: // voltage
          add2payload(data, 24, 8);
          break;
        case 4: // power factor
          add2payload(data, 32, 8);
          break;
      }
    }

    // set node id and sensor number (don't change here)
    payload[40] = uint8_t(i);
    payload[41] = node;
    /*
    for (int x = 0; x < sizeof(payload); x++) {
      Serial.print(char(payload[x]));
    }
    Serial.println();
    */
    // send to datalogger
    //sendData(loggerZbTx, loggerTxStatus);
    // wait 5 secs before sending next sensor
    delay(interval);
  }
}

void add2payload(char *dataString, int pos, int length) {
  for (int i = pos; i < pos + length; i++, dataString++) {
    payload[i] = *dataString;
    //Serial.print(*dataString);
  }
  //Serial.println();
}

void sendData(ZBTxRequest ZbTx, ZBTxStatusResponse TxStatus) {
  Serial.print("Sending...");
  xbee.send(ZbTx);
  Serial.println(" ");

  // after sending a tx request, we expect a status response
  // wait up to half second for the status response
  if (xbee.readPacket(500)) {
    // got a response!

    // should be a znet tx status
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(TxStatus);

      // get the delivery status, the fifth byte
      if (TxStatus.getDeliveryStatus() == SUCCESS) {
        // success. time to celebrate
        Serial.println("Received");
      } else {
        // the remote XBee did not receive our packet. is it powered on?
        Serial.println("Not received");
      }
    }
  } else if (xbee.getResponse().isError()) {
    Serial.print("Reading error: ");
    Serial.println(xbee.getResponse().getErrorCode());
  } else {
    // local XBee did not provide a timely TX Status Response -- should not happen
    Serial.println("Timeout");
  }
}
