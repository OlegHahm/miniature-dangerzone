#ifndef ORYX_SHA1_H
#define ORYX_SHA1_H
/**
 * @file sha1.h
 * @brief SHA-1 (Secure Hash Algorithm 1)
 *
 * @section License
 *
 * Copyright (C) 2010-2016 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneCrypto Open.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 1.7.0
 **/


//SHA-1 block size
#define SHA1_BLOCK_SIZE 64
//SHA-1 digest size
#define SHA1_DIGEST_SIZE 20

#define MIN(a, b)                    ((a) > (b) ? (b) : (a))

static inline uint32_t ROL32(uint32_t word, unsigned int shift)
{
	return (word << shift) | (word >> ((-shift) & 31));
}

/**
 * @brief SHA-1 algorithm context
 **/

typedef struct
{
   union
   {
      uint32_t h[5];
      uint8_t digest[20];
   };
   union
   {
      uint32_t w[16];
      uint8_t buffer[64];
   };
   size_t size;
   uint64_t totalSize;
} Sha1Context;



//SHA-1 related functions
void sha1Init(Sha1Context *context);
void sha1Update(Sha1Context *context, const void *data, size_t length);
void sha1Final(Sha1Context *context, uint8_t *digest);
void sha1ProcessBlock(Sha1Context *context);

#endif /* ORYX_SHA1_H */
