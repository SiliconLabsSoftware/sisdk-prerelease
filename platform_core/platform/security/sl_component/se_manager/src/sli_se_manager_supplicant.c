/***************************************************************************//**
 * @file
 * @brief Silicon Labs Secure Engine Manager API.
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

#include "sl_se_manager.h"

#if defined(SLI_MAILBOX_COMMAND_SUPPORTED)

  #include <string.h>
  #include "sli_se_manager_internal.h"
  #include "sli_se_manager_mailbox.h"
  #include "sli_se_manager_supplicant.h"

  sl_status_t sli_se_sae_prepare_commit(sl_se_command_context_t *cmd_ctx,
                                        sli_se_sae_context_t     *sae_ctx,
                                        const uint8_t           *mac_ap,
                                        const uint8_t           *mac_client,
                                        size_t                  mac_len,
                                        const uint8_t           *password,
                                        size_t                  password_len,
                                        bool                    h2e,
                                        const uint8_t           *identifier,
                                        size_t                  identifier_len,
                                        const uint8_t           *ssid,
                                        size_t                  ssid_len,
                                        uint32_t                iterations)
  {
    // Not used for now
    (void)iterations;

    // Check for invalid null pointer inputs
    if ((cmd_ctx == NULL) || (sae_ctx == NULL) || (mac_ap == NULL) || (mac_client == NULL) || (password == NULL)
        || ((h2e) && ((ssid == NULL) || ((identifier == NULL) && (identifier_len != 0))))) {
      return SL_STATUS_NULL_POINTER;
    }
    // Check for invalid null pointer inputs in sae_ctx
    if ((sae_ctx->ephemeral_ctx.pwe.pointer == NULL) || (sae_ctx->ephemeral_ctx.rand.pointer == NULL)
        || (sae_ctx->ephemeral_ctx.scalar == NULL) || (sae_ctx->ephemeral_ctx.element == NULL)) {
      return SL_STATUS_NULL_POINTER;
    }
    // Check for invalid length inputs
    if ((mac_len != 6) || password_len == 0
        || (sae_ctx->ephemeral_ctx.pwe.size != 64) || (sae_ctx->ephemeral_ctx.rand.size != 32)
        || (h2e && (ssid_len == 0))
        || (!h2e && (identifier_len != 0 || ssid_len != 0))) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    // Check for invalid group
    if (sae_ctx->group != SL_SE_KEY_TYPE_ECC_P256) {
      return SL_STATUS_INVALID_PARAMETER;
    }

    sl_status_t status;
    uint8_t dummy_priv_key[32] = { 0 };
    uint32_t command_word = SLI_SE_COMMAND_SAE_PREPARE_COMMIT | (h2e ? SLI_SE_COMMAND_OPTION_SAE_H2E : 0);
    sli_se_mailbox_command_t *se_cmd = &cmd_ctx->command;

    // Prepare the input key
    sl_se_key_descriptor_t key = {
      .type = SL_SE_KEY_TYPE_ECC_P256,
      .flags = SL_SE_KEY_FLAG_ASYMMETRIC_BUFFER_HAS_PRIVATE_KEY,
      .storage.method = SL_SE_KEY_STORAGE_EXTERNAL_PLAINTEXT,
      .storage.location.buffer.pointer = dummy_priv_key,
      .storage.location.buffer.size = sizeof(dummy_priv_key),
    };

    // Setup SE command and parameters
    sli_se_command_init(cmd_ctx, command_word);
    // Add key and length parameters to command
    sli_add_key_parameters(cmd_ctx, &key, status);
    sli_se_mailbox_command_add_parameter(se_cmd, password_len);
    if (h2e) {
      sli_se_mailbox_command_add_parameter(se_cmd, ssid_len);
      sli_se_mailbox_command_add_parameter(se_cmd, identifier_len);
    }

    // Add key metadata block to command
    sli_add_key_metadata(cmd_ctx, &key, status);
    // Add key input block to command
    sli_add_key_input(cmd_ctx, &key, status);

    // Prepare inputs
    sli_se_datatransfer_t mac_ap_buffer = SLI_SE_DATATRANSFER_DEFAULT(mac_ap, 6);
    sli_se_datatransfer_t mac_client_buffer = SLI_SE_DATATRANSFER_DEFAULT(mac_client, 6);
    sli_se_datatransfer_t password_buffer = SLI_SE_DATATRANSFER_DEFAULT(password, password_len);
    sli_se_datatransfer_t ssid_buffer = SLI_SE_DATATRANSFER_DEFAULT(ssid, ssid_len);
    sli_se_datatransfer_t identifier_buffer = SLI_SE_DATATRANSFER_DEFAULT(identifier, identifier_len);
    sli_se_mailbox_command_add_input(se_cmd, &mac_ap_buffer);
    sli_se_mailbox_command_add_input(se_cmd, &mac_client_buffer);
    sli_se_mailbox_command_add_input(se_cmd, &password_buffer);
    if (h2e) {
      sli_se_mailbox_command_add_input(se_cmd, &ssid_buffer);
      sli_se_mailbox_command_add_input(se_cmd, &identifier_buffer);
    }

    // Prepare outputs
    sli_se_datatransfer_t pwe_buffer = SLI_SE_DATATRANSFER_DEFAULT(sae_ctx->ephemeral_ctx.pwe.pointer, 64);
    sli_se_datatransfer_t rand_buffer = SLI_SE_DATATRANSFER_DEFAULT(sae_ctx->ephemeral_ctx.rand.pointer, 32);
    sli_se_datatransfer_t local_commit_scalar_buffer = SLI_SE_DATATRANSFER_DEFAULT(sae_ctx->ephemeral_ctx.scalar, 32);
    sli_se_datatransfer_t local_commit_element_buffer = SLI_SE_DATATRANSFER_DEFAULT(sae_ctx->ephemeral_ctx.element, 64);
    sli_se_mailbox_command_add_output(se_cmd, &pwe_buffer);
    sli_se_mailbox_command_add_output(se_cmd, &rand_buffer);
    sli_se_mailbox_command_add_output(se_cmd, &local_commit_scalar_buffer);
    sli_se_mailbox_command_add_output(se_cmd, &local_commit_element_buffer);
    
    return sli_se_execute_and_wait(cmd_ctx);
  }

  sl_status_t sli_se_sae_process_commit(sl_se_command_context_t *cmd_ctx,
                                        sli_se_sae_context_t     *sae_ctx,
                                        const uint8_t           *peer_scalar,
                                        const uint8_t           *peer_element)
  {
    // Check for invalid null pointer inputs
    if ((cmd_ctx == NULL) || (sae_ctx == NULL) || (peer_scalar == NULL) || (peer_element == NULL) ) {
      return SL_STATUS_NULL_POINTER;
    }
    // Check for null pointers in sae_ctx
    if ((sae_ctx->ephemeral_ctx.rand.pointer == NULL) || (sae_ctx->ephemeral_ctx.pwe.pointer == NULL)
    || (sae_ctx->ephemeral_ctx.kck.storage.location.buffer.pointer == NULL)
    || (sae_ctx->pmk.storage.location.buffer.pointer == NULL)) {
      return SL_STATUS_NULL_POINTER;
    }
    // Check for invalid length inputs
    if ((sae_ctx->ephemeral_ctx.rand.size != 32) || (sae_ctx->ephemeral_ctx.pwe.size != 64)
        || (sae_ctx->ephemeral_ctx.kck.storage.location.buffer.size != 32)
        || (sae_ctx->pmk.storage.location.buffer.size != 32)) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    // Check for invalid group and key storage methods
    if ((sae_ctx->group != SL_SE_KEY_TYPE_ECC_P256) || (sae_ctx->ephemeral_ctx.kck.storage.method != SL_SE_KEY_STORAGE_EXTERNAL_PLAINTEXT)
        || (sae_ctx->pmk.storage.method != SL_SE_KEY_STORAGE_EXTERNAL_PLAINTEXT)) {
      return SL_STATUS_INVALID_PARAMETER;
    }

    sl_status_t status;
    uint8_t dummy_priv_key[32] = { 0 };
    uint32_t command_word = SLI_SE_COMMAND_SAE_PROCESS_COMMIT;
    sli_se_mailbox_command_t *se_cmd = &cmd_ctx->command;

    // Prepare the key
    sl_se_key_descriptor_t key = {
      .type = SL_SE_KEY_TYPE_ECC_P256,
      .flags = SL_SE_KEY_FLAG_ASYMMETRIC_BUFFER_HAS_PRIVATE_KEY,
      .storage.method = SL_SE_KEY_STORAGE_EXTERNAL_PLAINTEXT,
      .storage.location.buffer.pointer = dummy_priv_key,
      .storage.location.buffer.size = sizeof(dummy_priv_key),
    };

    // Setup SE command and parameters
    sli_se_command_init(cmd_ctx, command_word);
    // Add key parameters to command
    sli_add_key_parameters(cmd_ctx, &key, status);
    // Add key metadata block to command
    sli_add_key_metadata(cmd_ctx, &key, status);
    // Add key input block to command
    sli_add_key_input(cmd_ctx, &key, status);

    // Prepare inputs
    sli_se_datatransfer_t local_commit_scalar_buffer = SLI_SE_DATATRANSFER_DEFAULT(sae_ctx->ephemeral_ctx.scalar, 32);
    sli_se_datatransfer_t peer_commit_scalar_buffer = SLI_SE_DATATRANSFER_DEFAULT(peer_scalar, 32);
    sli_se_datatransfer_t peer_commit_element_buffer = SLI_SE_DATATRANSFER_DEFAULT(peer_element, 64);
    sli_se_datatransfer_t rand_buffer = SLI_SE_DATATRANSFER_DEFAULT(sae_ctx->ephemeral_ctx.rand.pointer, 32);
    sli_se_datatransfer_t pwe_buffer = SLI_SE_DATATRANSFER_DEFAULT(sae_ctx->ephemeral_ctx.pwe.pointer, 64);
    sli_se_mailbox_command_add_input(se_cmd, &local_commit_scalar_buffer);
    sli_se_mailbox_command_add_input(se_cmd, &peer_commit_scalar_buffer);
    sli_se_mailbox_command_add_input(se_cmd, &peer_commit_element_buffer);
    sli_se_mailbox_command_add_input(se_cmd, &rand_buffer);
    sli_se_mailbox_command_add_input(se_cmd, &pwe_buffer);

    // Prepare outputs
    sli_se_datatransfer_t kck_buffer = SLI_SE_DATATRANSFER_DEFAULT(
      sae_ctx->ephemeral_ctx.kck.storage.location.buffer.pointer,
      32);
    sli_se_datatransfer_t pmk_buffer = SLI_SE_DATATRANSFER_DEFAULT(sae_ctx->pmk.storage.location.buffer.pointer, 32);
    sli_se_mailbox_command_add_output(se_cmd, &kck_buffer);
    sli_se_mailbox_command_add_output(se_cmd, &pmk_buffer);


    return sli_se_execute_and_wait(cmd_ctx);
  }

#endif // defined(SLI_MAILBOX_COMMAND_SUPPORTED)