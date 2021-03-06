/*
 * Copyright (c) 2013, Kenneth MacKay
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdint.h>

/* Create a public key from a private key.
 *
 * Inputs:
 *	private_key - Your private key.
 *
 * Outputs:
 *	public_key  - Will be filled in with the public key.
 *
 * Returns true if the public key was generated successfully, false
 * if an error occurred. The keys are with the LSB first.
 */
bool bluez554_ecc_make_public_key(const uint8_t private_key[32], uint8_t public_key[64]);

/* Create a public/private key pair.
 *
 * Outputs:
 *	public_key  - Will be filled in with the public key.
 *	private_key - Will be filled in with the private key.
 *
 * Returns true if the key pair was generated successfully, false
 * if an error occurred. The keys are with the LSB first.
 */
bool bluez554_ecc_make_key(uint8_t public_key[64], uint8_t private_key[32]);

/* Check to see if a public key is valid.
 *
 * Inputs:
 *	public_key - The public key to check.
 *
 * Returns true if the public key is valid, false if it is invalid.
*/
bool bluez554_ecc_valid_public_key(const uint8_t public_key[64]);

/* Compute a shared secret given your secret key and someone else's
 * public key.
 * Note: It is recommended that you hash the result of bluez554_ecdh_shared_secret
 * before using it for symmetric encryption or HMAC.
 *
 * Inputs:
 *	public_key  - The public key of the remote party.
 *	private_key - Your private key.
 *
 * Outputs:
 *	secret - Will be filled in with the shared secret value.
 *
 * Returns true if the shared secret was generated successfully, false
 * if an error occurred. Both input and output parameters are with the
 * LSB first.
 */
bool bluez554_ecdh_shared_secret(const uint8_t public_key[64],
				const uint8_t private_key[32],
				uint8_t secret[32]);
