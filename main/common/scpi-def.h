/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer
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

#ifndef __SCPI_DEF_H_
#define __SCPI_DEF_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "scpi/scpi.h"
#include "status-def.h"


#define SCPI_INPUT_BUFFER_LENGTH                    256
#define SCPI_OUTPUT_BUFFER_LENGTH 					256
#define SCPI_ERROR_QUEUE_SIZE 17
#define SCPI_IDN1 "Vox Power"

#define BUFFER_SIZE 64

extern const scpi_command_t scpi_commands[];
extern scpi_interface_t scpi_socket_interface, scpi_usbtmc_interface,scpi_http_interface;
extern scpi_t scpi_TCP_context, scpi_usbtmc_context, scpi_http_context;

extern char scpi_http_input_buffer[];
extern char scpi_tcp_input_buffer[];
extern char scpi_usbtmc_input_buffer[];
extern scpi_error_t scpi_error_queue_data[];

size_t SCPI_http_Write(scpi_t * context, const char * data, size_t len);
int SCPI_http_Error(scpi_t * context, int_fast16_t err);
scpi_result_t SCPI_http_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
scpi_result_t SCPI_http_Reset(scpi_t * context);
scpi_result_t SCPI_http_Flush(scpi_t * context);

size_t SCPI_socket_Write(scpi_t * context, const char * data, size_t len);
int SCPI_socket_Error(scpi_t * context, int_fast16_t err);
scpi_result_t SCPI_socket_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
scpi_result_t SCPI_socket_Reset(scpi_t * context);
scpi_result_t SCPI_socket_Flush(scpi_t * context);

size_t SCPI_USBTMC_Write(scpi_t * context, const char * data, size_t len);
int SCPI_USBTMC_Error(scpi_t * context, int_fast16_t err);
scpi_result_t SCPI_USBTMC_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
scpi_result_t SCPI_USBTMC_Reset(scpi_t * context);
scpi_result_t SCPI_USBTMC_Flush(scpi_t * context);



#ifdef __cplusplus
}
#endif
#endif /* __SCPI_DEF_H_ */
