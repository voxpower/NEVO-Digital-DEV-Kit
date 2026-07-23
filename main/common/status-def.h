/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2026, Vox Power Ltd
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   status-def.h
 * @date   Thu Dec 11 10:33:45 UTC 2025
 * 
 * @brief  Implementation of custom status definitions
 * 
 * 
 */

#ifndef __STATUS_DEF_H_
#define	__STATUS_DEF_H_

#ifdef  __cplusplus
extern "C" {
#endif


#define ISUM_CV 0x01    /* Constant Voltage Mode */
#define ISUM_CC 0x02    /* Constant Current Mode */
#define ISUM_OVW 0x04    /* Over Voltage Warning */
#define ISUM_UVW 0x08    /* Under Voltage Warning */
#define ISUM_TON 0x10    /* Startup time warning */
#define ISUM_OTW 0x20    /* Over Temperture Warning */
#define ISUM_INH 0x40    /* Unit off */


#ifdef  __cplusplus
}
#endif

#endif	/* SCPI_IEEE488_H */

