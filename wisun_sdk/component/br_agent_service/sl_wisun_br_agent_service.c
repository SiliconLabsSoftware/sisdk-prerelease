/***************************************************************************//**
 * @file sl_wisun_br_agent_service.c
 * @brief Wi-SUN Border Router Agent Service
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>

#include "sl_assert.h"
#include "sl_string.h"
#include "cmsis_os2.h"
#include "sl_cmsis_os2_common.h"
#include "socket/socket.h"
#include "sl_memory_manager.h"
#include "sl_wisun_types.h"
#include "border_router/sl_wisun_br_api.h"
#include "sl_wisun_br_agent_service_config.h"
#include "sl_wisun_br_agent_service.h"
#include "sl_wisun_app_core.h"
#include "sl_wisun_app_br_core.h"
#include "sl_wisun_app_setting_br.h"
#include "errno.h"

#ifdef SL_CATALOG_WISUN_BR_DHCPV6_SERVER_PRESENT
#include "sl_wisun_br_dhcpv6_server.h"
#endif
// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
// Free a pointer allocated via sl_malloc
#define __cleanup(ptr)                                                          \
  do {                                                                          \
    if ((ptr) != NULL) {                                                        \
      sl_memory_region_t __heap = sl_memory_get_heap_region();                  \
      void *__p = (void *)(ptr);                                                \
      if (__p >= __heap.addr &&                                                 \
          (uintptr_t)__p < ((uintptr_t)__heap.addr + (uintptr_t)__heap.size)) { \
        sl_free(__p);                                                           \
      }                                                                         \
      (ptr) = NULL;                                                             \
    }                                                                           \
  } while (0)

// Agent Service Thread stack size in words
#define SL_WISUN_BR_AGENT_SERVICE_STACK_SIZE_WORD         (256UL)

// Agent Service buffer size
#define SL_WISUN_BR_AGENT_SERVICE_BUFF_SIZE               (2048U)

// Agent Service Request/Response codes
#define SL_WISUN_BR_AGENT_SERVICE_CODE_GET_TOPOLOGY       (0x01U)
#define SL_WISUN_BR_AGENT_SERVICE_CODE_GET_CONFIG_PARAMS  (0x02U)
#define SL_WISUN_BR_AGENT_SERVICE_CODE_SET_CONFIG_PARAMS  (0x03U)
#define SL_WISUN_BR_AGENT_SERVICE_CODE_RESTART_BR         (0x04U)
#define SL_WISUN_BR_AGENT_SERVICE_CODE_STOP_BR            (0x05U)

// Agent Service message type
typedef struct sl_wisun_br_agent_service_msg {
  // Request/Response code
  uint32_t msg_code;
  // Payload length
  uint32_t payload_len;
  // Payload data
  uint8_t *payload;
} sl_wisun_br_agent_service_msg_t;
// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
/**************************************************************************//**
 * @brief Agent Service task function
 * @details This function is the main task for the Agent Service
 *
 * @param[in] args Arguments
 *****************************************************************************/
static void _agent_service_task_fnc(void *args);

/**************************************************************************//**
 * @brief Parse received TCP message
 * @details This function parses the received TCP message
 *
 * @param[in] buff Pointer to the received message buffer
 * @param[in] buff_len Length of the received message buffer
 * @param[out] parsed_msg Pointer to the output message structure
 * @return SL_STATUS_OK on success, error code otherwise
 *****************************************************************************/
static sl_status_t _parse_received_msg(const uint8_t * const buff,
                                       uint32_t buff_len,
                                       sl_wisun_br_agent_service_msg_t * const parsed_msg);

/**************************************************************************//**
 * @brief Create and send response message
 * @details This function creates and sends the response message based on the request type
 *
 * @param[in] parsed_msg Pointer to the parsed message structure
 * @param[in] sockid Socket ID
 * @return SL_STATUS_OK on success, error code otherwise
 *****************************************************************************/
static sl_status_t _create_and_send_resp_msg(const sl_wisun_br_agent_service_msg_t * const parsed_msg,
                                             int32_t sockid);

/**************************************************************************//**
 * @brief Send message
 * @details This function sends a message to the specified socket
 *
 * @param[in] sockid Socket ID
 * @param[in] resp_msg Pointer to the response message structure
 * @return SL_STATUS_OK on success, error code otherwise
 *****************************************************************************/
static sl_status_t _send_msg(int32_t sockid,
                             const sl_wisun_br_agent_service_msg_t * const resp_msg);

/**************************************************************************//**
 * @brief Get network topology
 * @details This function retrieves the network topology information
 *
 * @param[out] resp_msg Pointer to the response message structure
 * @return SL_STATUS_OK on success, error code otherwise
 *****************************************************************************/
static sl_status_t _get_network_topology(sl_wisun_br_agent_service_msg_t * const resp_msg);

/**************************************************************************//**
 * @brief Get configuration parameters
 * @details This function retrieves the configuration parameters information
 *
 * @param[out] resp_msg Pointer to the response message structure
 * @return SL_STATUS_OK on success, error code otherwise
 *****************************************************************************/
static sl_status_t _get_config_params(sl_wisun_br_agent_service_msg_t * const resp_msg);

/**************************************************************************//**
 * @brief Set configuration parameters
 * @details This function sets the configuration parameters information
 *
 * @param[in] parsed_msg Pointer to the parsed message structure
 * @return SL_STATUS_OK on success, error code otherwise
 *****************************************************************************/
static sl_status_t _set_config_params(const sl_wisun_br_agent_service_msg_t * const parsed_msg);

/**************************************************************************//**
 * @brief Check if a pointer is within a buffer
 *
 * @param ptr Pointer to check
 * @param buff_ptr Pointer to the buffer
 * @param buff_size Size of the buffer
 * @return bool True if the pointer is within the buffer, false otherwise
 *****************************************************************************/
__STATIC_INLINE bool _is_ptr_in_buff(const uint8_t * const ptr,
                                     const uint8_t * const buff_ptr,
                                     const uint16_t buff_size);

/**************************************************************************//**
 * @brief Acquire Agent Service mutex
 * @details Internal mutex lock
 *****************************************************************************/
__STATIC_INLINE void _agent_service_mutex_acquire(void);

/**************************************************************************//**
 * @brief Release Agent Service mutex
 * @details Internal mutex release
 *****************************************************************************/
__STATIC_INLINE void _agent_service_mutex_release(void);
// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
// Agent Service task ID
static osThreadId_t _agent_service_task = NULL;

// Agent Service task attributes
static const osThreadAttr_t _agent_service_task_attr = {
  .name       = "AgentServiceTask",
  .attr_bits  = osThreadDetached,
  .cb_mem     = NULL,
  .cb_size    = 0UL,
  .stack_mem  = NULL,
  .stack_size = (SL_WISUN_BR_AGENT_SERVICE_STACK_SIZE_WORD * sizeof(void *)) & 0xFFFFFFF8U,
  .priority   = osPriorityNormal1,
  .tz_module  = 0UL,
  .reserved   = 0UL
};

// Remote address of the host Agent Service
static sockaddr_in6_t _remote_addr = {
  .sin6_family = AF_INET6,
  .sin6_addr = { .address = { 0U } }
};

// Agent Service mutex
static osMutexId_t _agent_service_mtx = NULL;

// Agent Service mutex attribute
static const osMutexAttr_t _agent_service_mtx_attr = {
  .name      = "AgentService_mtx",
  .attr_bits = osMutexRecursive,
  .cb_mem    = NULL,
  .cb_size   = 0UL
};
// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
void sl_wisun_br_agent_service_init(void)
{
   // Create mutex
  _agent_service_mtx = osMutexNew(&_agent_service_mtx_attr);
  EFM_ASSERT(_agent_service_mtx != NULL);

  // Create Agent Service thread
  _agent_service_task = osThreadNew(_agent_service_task_fnc,
                                    NULL,
                                    &_agent_service_task_attr);
  EFM_ASSERT(_agent_service_task != NULL);

  // Initialize remote address port
  _remote_addr.sin6_port = htons(SL_WISUN_BR_AGENT_SERVICE_REMOTE_HOST_PORT);
}

void sl_wisun_br_agent_service_send_graph_info(sl_wisun_evt_t *evt)
{
  sl_wisun_br_agent_service_msg_t resp_msg = { 0 };
  int32_t sockid = SOCKET_INVALID_ID;

  switch (evt->evt.br_routing_table_update.event) {
    case SL_WISUN_ROUTING_TABLE_UPDATE_ROUTE_CHANGED:
      // get new network topology
      if (_get_network_topology(&resp_msg) != SL_STATUS_OK) {
        return;
      }
      break;
    default:
      return;
  }

  // send updated network topology to the remote Agent Service host
  // create client socket
  sockid = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
  if (sockid == SOCKET_INVALID_ID) {
    __cleanup(resp_msg.payload);
    return;
  }
  // setting the server address
  if (inet_pton(AF_INET6,
                SL_WISUN_BR_AGENT_SERVICE_DEFAULT_REMOTE_ADDR,
                &_remote_addr.sin6_addr) != 1) {
    __cleanup(resp_msg.payload);
    close(sockid);
    return;
  }
  // connect to the server
  if (connect(sockid, (const struct sockaddr *)&_remote_addr,
              sizeof(_remote_addr)) == SOCKET_RETVAL_ERROR && errno != EINPROGRESS) {
    __cleanup(resp_msg.payload);
    close(sockid);
    return;
  }

  // wait for connection to be established
  osDelay(1000U);

  // send the response message and cleanup
  (void)_send_msg(sockid, &resp_msg);
  __cleanup(resp_msg.payload);
  close(sockid);
}

sl_status_t sl_wisun_br_agent_service_set_remote_addr(const char *remote_address,
                                                      const uint16_t port)
{
  sl_status_t result = SL_STATUS_OK;
  int32_t ip_result = 0;

  _agent_service_mutex_acquire();
  if (remote_address == NULL) {
    result =  SL_STATUS_NULL_POINTER;
  } else {
    ip_result = inet_pton(AF_INET6,
                          remote_address,
                          &_remote_addr.sin6_addr);
    if (ip_result != 1) {
      result = SL_STATUS_FAIL;
    } else {
      _remote_addr.sin6_port = htons(port);
    }
  }
  _agent_service_mutex_release();

  return result;
}

void sl_wisun_br_agent_service_send_reg(void)
{
  sl_wisun_br_agent_service_msg_t resp_msg = { 0 };
  int32_t sockid = SOCKET_INVALID_ID;

 // get new network topology
 if (_get_config_params(&resp_msg) != SL_STATUS_OK) {
    return;
 }

  // change message code to set config params
  resp_msg.msg_code = SL_WISUN_BR_AGENT_SERVICE_CODE_SET_CONFIG_PARAMS;
  // create client socket
  sockid = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
  if (sockid == SOCKET_INVALID_ID) {
    return;
  }
  // setting the server address
  if (inet_pton(AF_INET6,
                SL_WISUN_BR_AGENT_SERVICE_DEFAULT_REMOTE_ADDR,
                &_remote_addr.sin6_addr) != 1) {
    close(sockid);
    return;
  }
  // connect to the server
  if (connect(sockid, (const struct sockaddr *)&_remote_addr,
              sizeof(_remote_addr)) == SOCKET_RETVAL_ERROR && errno != EINPROGRESS) {
    close(sockid);
    return;
  }

  // wait for connection to be established
  osDelay(1000U);

  // send the response message and cleanup
  (void)_send_msg(sockid, &resp_msg);
  __cleanup(resp_msg.payload);
  close(sockid);
}
// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
static void _agent_service_task_fnc(void *args)
{
  static uint8_t buff[SL_WISUN_BR_AGENT_SERVICE_BUFF_SIZE] = { 0U };
  static sockaddr_in6_t srv_addr = { 0 };
  static sockaddr_in6_t clnt_addr = { 0 };
  int32_t srv_sockid = SOCKET_INVALID_ID;
  int32_t clnt_sockid = SOCKET_INVALID_ID;
  int32_t r = SOCKET_RETVAL_ERROR;
  socklen_t clnt_addr_len = sizeof(clnt_addr);
  sl_wisun_br_agent_service_msg_t recv_msg = { 0 };

  (void) args;

  // create TCP socket
  srv_sockid = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  EFM_ASSERT(srv_sockid != SOCKET_INVALID_ID);

  // fill the server address structure
  srv_addr.sin6_family = AF_INET6;
  srv_addr.sin6_addr = in6addr_any;
  srv_addr.sin6_port = htons(SL_WISUN_BR_AGENT_SERVICE_SERVER_PORT);

  // bind address to the socket
  r = bind(srv_sockid,
           (const struct sockaddr *) &srv_addr,
           sizeof(sockaddr_in6_t));
  EFM_ASSERT(r != SOCKET_RETVAL_ERROR);

  // listen on socket
  r = listen(srv_sockid, 0);
  EFM_ASSERT(r != SOCKET_RETVAL_ERROR);

  // waiting for connection request
  SL_WISUN_BR_AGENT_SERVICE_LOOP {
    // accept incoming connections
    clnt_sockid = accept(srv_sockid,
                         (struct sockaddr *) &clnt_addr,
                         &clnt_addr_len);
    if (clnt_sockid == SOCKET_INVALID_ID) {
      continue;
    }
    // receiver loop
    r = recv(clnt_sockid, buff, sizeof(buff) - 1U, 0);
    switch (r) {
      case SOCKET_RETVAL_ERROR: // error
        break;

      case 0L: // socket closed, EOF
        break;

      default: // default: data received
        buff[r] = '\0';
        // parse the received message
        if (_parse_received_msg(buff,
                                (size_t)r,
                                &recv_msg) != SL_STATUS_OK) {
          break;
        }
        // create and send response message
        if (_create_and_send_resp_msg(&recv_msg,
                                      clnt_sockid) != SL_STATUS_OK) {
          break;
        }
        break;
    }
    close(clnt_sockid);
    osDelay(1UL);
  }
}

static sl_status_t _parse_received_msg(const uint8_t * const buff,
                                       uint32_t buff_len,
                                       sl_wisun_br_agent_service_msg_t * const parsed_msg)
{
  const uint8_t *ptr = buff;

  if ((buff == NULL)
      || (buff_len == 0U)
      || (parsed_msg == NULL)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // parse request/response code
  parsed_msg->msg_code = ntohl(*((uint32_t *)ptr));
  ptr += sizeof(uint32_t);
  if (!_is_ptr_in_buff(ptr, buff, buff_len)) {
    return SL_STATUS_FAIL;
  }

  // parse payload length
  parsed_msg->payload_len = ntohl(*((uint32_t *)ptr));

  // parse payload if present
  if (parsed_msg->payload_len > 0U) {
    ptr += sizeof(uint32_t);
    if (!_is_ptr_in_buff(ptr, buff, buff_len)) {
      return SL_STATUS_FAIL;
    }
    parsed_msg->payload = (uint8_t *)ptr;
  }

  return SL_STATUS_OK;
}

static sl_status_t _create_and_send_resp_msg(const sl_wisun_br_agent_service_msg_t * const parsed_msg,
                                             int32_t sockid)
{
  sl_status_t ret = SL_STATUS_OK;
  sl_wisun_br_agent_service_msg_t resp_msg = { 0 };
  sl_wisun_br_state_t br_state = SL_WISUN_BR_STATE_INITIALIZED;

  if (parsed_msg == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // process get/set request
  switch (parsed_msg->msg_code) {
    case SL_WISUN_BR_AGENT_SERVICE_CODE_GET_TOPOLOGY:
      if (_get_network_topology(&resp_msg) != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
      }
      break;

    case SL_WISUN_BR_AGENT_SERVICE_CODE_GET_CONFIG_PARAMS:
      if (_get_config_params(&resp_msg) != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
      }
      break;

    case SL_WISUN_BR_AGENT_SERVICE_CODE_SET_CONFIG_PARAMS:
      if (_set_config_params(parsed_msg) != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
      }
      return SL_STATUS_OK;

    case SL_WISUN_BR_AGENT_SERVICE_CODE_RESTART_BR:
      if (sl_wisun_br_stop() != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
      }
    #ifdef SL_CATALOG_WISUN_BR_DHCPV6_SERVER_PRESENT
      (void)sl_wisun_br_dhcpv6_server_stop();
    #endif
      osDelay(1000U);
      if ((sl_wisun_br_get_state(&br_state) == SL_STATUS_OK)
          && (br_state != SL_WISUN_BR_STATE_OPERATIONAL)) {
        sl_wisun_app_br_core_start();
        return SL_STATUS_OK;
      } else {
        // BR is already running
        return SL_STATUS_FAIL;
      }

    case SL_WISUN_BR_AGENT_SERVICE_CODE_STOP_BR:
      if (sl_wisun_br_stop() != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
      }
    #ifdef SL_CATALOG_WISUN_BR_DHCPV6_SERVER_PRESENT
      (void)sl_wisun_br_dhcpv6_server_stop();
    #endif
      return SL_STATUS_OK;

    default:
      return SL_STATUS_NOT_SUPPORTED;
  }

  // send the response message
  ret = _send_msg(sockid, &resp_msg);

  // cleanup
  __cleanup(resp_msg.payload);

  return ret;
}

static sl_status_t _send_msg(int32_t sockid,
                             const sl_wisun_br_agent_service_msg_t * const resp_msg)
{
  sl_status_t ret = SL_STATUS_OK;
  uint8_t *buff = NULL;
  uint8_t *ptr = NULL;
  uint32_t total_msg_size = 0U;

  if (resp_msg == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // calculate total message size
  total_msg_size = sizeof(resp_msg->msg_code)
                          + sizeof(resp_msg->payload_len)
                          + resp_msg->payload_len;

  // allocate buffer for response message
  buff = (uint8_t *)sl_malloc(total_msg_size);
  if (!buff) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  // clear buffer
  memset(buff, 0U, total_msg_size);

  // fill buffer with response message structure
  ptr = buff;

  // copy response code
  *((uint32_t *)ptr) = htonl(resp_msg->msg_code);
  ptr += sizeof(uint32_t);

  // copy payload length
  *((uint32_t *)ptr) = htonl(resp_msg->payload_len);
  ptr += sizeof(uint32_t);

  // copy payload if present
  if ((resp_msg->payload_len > 0U) && (resp_msg->payload != NULL)) {
    memcpy(ptr, resp_msg->payload, resp_msg->payload_len);
  }

  // send the response message
  if (send(sockid, buff, total_msg_size, 0) == SOCKET_RETVAL_ERROR) {
    ret = SL_STATUS_FAIL;
  }

  // cleanup buffer
  __cleanup(buff);

  return ret;
}

static sl_status_t _get_network_topology(sl_wisun_br_agent_service_msg_t * const resp_msg)
{
  sl_wisun_br_routing_table_entry_t *routing_table = NULL;
  uint16_t routing_table_size = 0U;
  static sl_wisun_ip_address_t ll_addr = { 0 };
  static sl_wisun_ip_address_t gua_addr = { 0 };
  static sl_wisun_ip_address_t dodagid_addr = { 0 };

  if (resp_msg == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // set response code
  resp_msg->msg_code = SL_WISUN_BR_AGENT_SERVICE_CODE_GET_TOPOLOGY;

  // get the number of routing table entries
  if (sl_wisun_br_get_routing_table_entry_count(&routing_table_size) != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }

  // include the BR itself
  routing_table_size += 1U;

  // allocate memory for the routing table
  routing_table = (sl_wisun_br_routing_table_entry_t *)sl_malloc(routing_table_size
                                                                 * sizeof(sl_wisun_br_routing_table_entry_t));
  if (!routing_table) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  // set payload length
  resp_msg->payload_len = routing_table_size * sizeof(sl_wisun_br_routing_table_entry_t);

  // clear routing table
  memset(routing_table, 0, routing_table_size * sizeof(sl_wisun_br_routing_table_entry_t));

  // add the BR entry as the first entry
  (void)sl_wisun_br_get_ip_addresses(ll_addr.address, gua_addr.address, dodagid_addr.address);
  memcpy(routing_table->target.address, gua_addr.address, sizeof(routing_table->target.address));
  memset(routing_table->preferred.address, 0U, sizeof(routing_table->preferred.address));
  memset(routing_table->backup.address, 0U, sizeof(routing_table->backup.address));

  // get the routing table entries
  if (routing_table_size > 1U) {
    routing_table_size -= 1U;
    if (sl_wisun_br_get_routing_table(&routing_table_size,
                                      &routing_table[1]) != SL_STATUS_OK) {
      __cleanup(routing_table);
      return SL_STATUS_FAIL;
    }
  }

  // set payload
  resp_msg->payload = (uint8_t *)routing_table;

  return SL_STATUS_OK;
}

static sl_status_t _get_config_params(sl_wisun_br_agent_service_msg_t * const resp_msg)
{
  static app_setting_br_t br_settings = { 0 };

  if (resp_msg == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // get current BR settings
  if (app_wisun_setting_br_get(&br_settings) != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }

  resp_msg->msg_code = SL_WISUN_BR_AGENT_SERVICE_CODE_GET_CONFIG_PARAMS;
  resp_msg->payload = (uint8_t *)&br_settings;
  resp_msg->payload_len = sizeof(app_setting_br_t);

  return SL_STATUS_OK;
}

static sl_status_t _set_config_params(const sl_wisun_br_agent_service_msg_t * const parsed_msg)
{
  sl_status_t ret = SL_STATUS_OK;
  app_setting_br_t *new_settings = NULL;
  static app_setting_br_t br_settings = { 0 };

  if (!parsed_msg || !parsed_msg->payload) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // get current BR settings
  if (app_wisun_setting_br_get(&br_settings) != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }

  // preserve PAN ID
  new_settings = (app_setting_br_t *)parsed_msg->payload;
  new_settings->pan_id = br_settings.pan_id;

  ret = app_wisun_setting_br_set(new_settings);

  return ret;
}

__STATIC_INLINE bool _is_ptr_in_buff(const uint8_t * const ptr,
                                     const uint8_t * const buff_ptr,
                                     const uint16_t buff_size)
{
  return (bool) ((ptr >= buff_ptr) && (ptr < (buff_ptr + buff_size)));
}

__STATIC_INLINE void _agent_service_mutex_acquire(void)
{
  EFM_ASSERT(osMutexAcquire(_agent_service_mtx, osWaitForever) == osOK);
}

__STATIC_INLINE void _agent_service_mutex_release(void)
{
  EFM_ASSERT(osMutexRelease(_agent_service_mtx) == osOK);
}
