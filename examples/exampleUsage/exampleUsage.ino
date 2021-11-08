/*
 * I2C-Generator: 0.2.0
 * Yaml Version: 0.1.0
 * Template Version: local build
 */
/*
 * Copyright (c) 2021, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <Arduino.h>
#include <NOxGasIndexAlgorithm.h>
#include <SensirionI2CSgp41.h>
#include <SensirionI2CSht4x.h>
#include <VOCGasIndexAlgorithm.h>
#include <Wire.h>

SensirionI2CSgp41 sgp41;
SensirionI2CSht4x sht4x;
VOCGasIndexAlgorithm voc_algorithm = VOCGasIndexAlgorithm();
NOxGasIndexAlgorithm nox_algorithm = NOxGasIndexAlgorithm();

// time in seconds needed for NOx conditioning
uint16_t conditioning_s = 10;

void setup() {

    Serial.begin(115200);
    while (!Serial) {
        delay(100);
    }

    Wire.begin();

    sgp41.begin(Wire);
    sht4x.begin(Wire);

    delay(1000);  // needed on some Arduino boards in order to have Serial ready

    int32_t index_offset;
    int32_t learning_time_offset_hours;
    int32_t learning_time_gain_hours;
    int32_t gating_max_duration_minutes;
    int32_t std_initial;
    int32_t gain_factor;
    voc_algorithm.get_tuning_parameters(
        index_offset, learning_time_offset_hours, learning_time_gain_hours,
        gating_max_duration_minutes, std_initial, gain_factor);

    Serial.println("\nVOC Index parameters");
    Serial.print("Index offset:\t");
    Serial.println(index_offset);
    Serial.print("Learing time offset hours:\t");
    Serial.println(learning_time_offset_hours);
    Serial.print("Learing time gain hours:\t");
    Serial.println(learning_time_gain_hours);
    Serial.print("Gating max duration minutes:\t");
    Serial.println(gating_max_duration_minutes);
    Serial.print("Std inital:\t");
    Serial.println(std_initial);
    Serial.print("Gain factor:\t");
    Serial.println(gain_factor);

    nox_algorithm.get_tuning_parameters(
        index_offset, learning_time_offset_hours, learning_time_gain_hours,
        gating_max_duration_minutes, std_initial, gain_factor);

    Serial.println("\nNOx Index parameters");
    Serial.print("Index offset:\t");
    Serial.println(index_offset);
    Serial.print("Learing time offset hours:\t");
    Serial.println(learning_time_offset_hours);
    Serial.print("Gating max duration minutes:\t");
    Serial.println(gating_max_duration_minutes);
    Serial.print("Gain factor:\t");
    Serial.println(gain_factor);
    Serial.println("");
}

void loop() {
    uint16_t error;
    char errorMessage[256];
    uint16_t srawRhTicks = 0;    // in ticks as defined by SHT4x
    uint16_t srawTempTicks = 0;  // in ticks as defined by SHT4x
    uint16_t srawVoc = 0;
    uint16_t srawNox = 0;
    uint16_t defaultCompenstaionRh = 0x8000;  // in ticks as defined by SGP41
    uint16_t defaultCompenstaionT = 0x6666;   // in ticks as defined by SGP41
    uint16_t compensation_RH = 0;             // in ticks as defined by SGP41
    uint16_t compensation_T = 0;              // in ticks as defined by SGP41

    delay(1000);

    error = sht4x.measureHighPrecisionTicks(srawTempTicks, srawRhTicks);
    if (error) {
        Serial.print(
            "SHT4x - Error trying to execute measureHighPrecision(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        Serial.println("Fallback to use default values for humidity and "
                       "temperature compensation for SGP41");
        compensation_RH = defaultCompenstaionRh;
        compensation_T = defaultCompenstaionT;
    } else {
        float temperature =
            static_cast<float>(srawTempTicks * 175.0 / 65535.0 - 45.0);
        float humidity = static_cast<float>(srawRhTicks * 125.0 / 65535.0 - 6);
        Serial.print("T:");
        Serial.print(temperature);
        Serial.print("\t");
        Serial.print("RH:");
        Serial.println(humidity);

        // convert temperature and humidity from ticks returned by SHT4x to
        // ticks as defined by SGP41 interface temperature ticks are identical
        // on SHT4x and SGP41
        compensation_T = srawTempTicks;
        compensation_RH = (srawRhTicks * 125 / 65535 - 6) * 65535 / 100;
    }

    if (conditioning_s > 0) {
        // During NOx conditioning (10s) SRAW NOx will remain 0
        error =
            sgp41.executeConditioning(compensation_RH, compensation_T, srawVoc);
        conditioning_s--;
    } else {
        error = sgp41.measureRawSignals(compensation_RH, compensation_T,
                                        srawVoc, srawNox);
    }

    if (error) {
        Serial.print("SGP41 - Error trying to execute measureRaw(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        int32_t voc_index = voc_algorithm.process(srawVoc);
        int32_t nox_index = voc_algorithm.process(srawNox);
        Serial.print("VOC Index: ");
        Serial.print(voc_index);
        Serial.print("\t");
        Serial.print("NOx Index: ");
        Serial.println(nox_index);
    }
}
