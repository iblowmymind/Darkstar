/*
    BSD 2-Clause License

    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ethernet/crc32.h"

namespace darkstar {

uint32_t CRC32::crc_table_[256] = {};
bool CRC32::table_initialized_ = false;

void CRC32::initialize_table() {
    if (table_initialized_) {
        return;
    }

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t temp = i;

        for (int j = 8; j > 0; j--) {
            if ((temp & 1) == 1) {
                temp = (temp >> 1) ^ kPolynomial;
            } else {
                temp >>= 1;
            }
        }

        crc_table_[i] = temp;
    }

    table_initialized_ = true;
}

CRC32::CRC32() {
    initialize_table();
    reset();
}

void CRC32::reset() {
    checksum_ = 0xffffffff;
}

void CRC32::add_to_checksum(uint16_t word) {
    uint8_t bytes[2];
    bytes[0] = static_cast<uint8_t>(word >> 8);
    bytes[1] = static_cast<uint8_t>(word);

    for (int i = 0; i < 2; i++) {
        uint8_t index = static_cast<uint8_t>((checksum_ ^ bytes[i]) & 0xff);
        checksum_ = (checksum_ >> 8) ^ crc_table_[index];
    }
}

} // namespace darkstar
