// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <azure/iot/az_iot_adu_ota.h>
#include <azure/iot/az_iot_hub_client_properties.h>

#include <azure/core/internal/az_precondition_internal.h>
#include <azure/core/internal/az_result_internal.h>


// TODO: rename and re-organize

/* Define the ADU agent component name.  */
#define AZ_IOT_ADU_OTA_AGENT_COMPONENT_NAME                           "deviceUpdate"

/* Define the ADU agent interface ID.  */
#define AZ_IOT_ADU_OTA_AGENT_INTERFACE_ID                             "dtmi:azure:iot:deviceUpdate;1"

/* Define the compatibility.  */
#define AZ_IOT_ADU_OTA_AGENT_COMPATIBILITY                            "manufacturer,model"

/* Define the ADU agent property name "agent" and sub property names.  */
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_AGENT                      "agent"

#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_DEVICEPROPERTIES           "deviceProperties"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_MANUFACTURER               "manufacturer"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_MODEL                      "model"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_INTERFACE_ID               "interfaceId"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ADU_VERSION                "aduVer"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_DO_VERSION                 "doVer"

#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_COMPAT_PROPERTY_NAMES      "compatPropertyNames"

#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_INSTALLED_CONTENT_ID       "installedUpdateId"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_PROVIDER                   "provider"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_NAME                       "name"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_VERSION                    "version"

#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_LAST_INSTALL_RESULT        "lastInstallResult"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RESULT_CODE                "resultCode"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_EXTENDED_RESULT_CODE       "extendedResultCode"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RESULT_DETAILS             "resultDetails"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_STEP_RESULTS               "stepResults"

#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_STATE                      "state"

/* Define the ADU agent property name "service" and sub property names.  */
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_SERVICE                    "service"

#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_WORKFLOW                   "workflow"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ACTION                     "action"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ID                         "id"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RETRY_TIMESTAMP            "retryTimestamp"

#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_UPDATE_MANIFEST            "updateManifest"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_UPDATE_MANIFEST_SIGNATURE  "updateManifestSignature"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_FILEURLS                   "fileUrls"

#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_MANIFEST_VERSION           "manifestVersion"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_UPDATE_ID                  "updateId"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_COMPATIBILITY              "compatibility"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_DEVICE_MANUFACTURER        "deviceManufacturer"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_DEVICE_MODEL               "deviceModel"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_GROUP                      "group"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_INSTRUCTIONS               "instructions"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_STEPS                      "steps"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_TYPE                       "type"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_HANDLE                     "handler"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_FILES                      "files"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_DETACHED_MANIFEST_FILED    "detachedManifestFileId"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_INSTALLED_CRITERIA         "installedCriteria"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_FILE_NAME                  "fileName"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_SIZE_IN_BYTES              "sizeInBytes"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_HASHES                     "hashes"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_SHA256                     "sha256"
#define AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_CREATED_DATE_TIME          "createdDateTime"

#define NULL_TERM_CHAR_SIZE                                             1

#define RETURN_IF_JSON_TOKEN_TYPE_NOT(jr_ptr, json_token_type) \
    if (jr_ptr->token.kind != json_token_type)                 \
    {                                                          \
        return AZ_ERROR_JSON_INVALID_STATE;                    \
    }

#define RETURN_IF_JSON_TOKEN_TEXT_NOT(jr_ptr, literal_text)    \
    if (!az_json_token_is_text_equal(&jr_ptr->token,           \
            AZ_SPAN_FROM_STR(literal_text)))                   \
    {                                                          \
        return AZ_ERROR_JSON_INVALID_STATE;                    \
    }

#define trim_null_terminator(azspan) \
  az_span_slice(azspan, 0, az_span_size(azspan) - NULL_TERM_CHAR_SIZE)

#define safe_add_json_property_int32(json_writer, propName, propValue) \
    _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&json_writer, \
        AZ_SPAN_FROM_STR(propName))); \
    _az_RETURN_IF_FAILED(az_json_writer_append_int32(&json_writer, propValue));

#define safe_add_json_property_string(json_writer, propName, propValue) \
    _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&json_writer, \
        AZ_SPAN_FROM_STR(propName))); \
    _az_RETURN_IF_FAILED(az_json_writer_append_string( \
        &json_writer, AZ_SPAN_FROM_STR(propValue)));

#define safe_add_json_property_az_span(json_writer, propName, propValue) \
    _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&json_writer, \
        AZ_SPAN_FROM_STR(propName))); \
    _az_RETURN_IF_FAILED(az_json_writer_append_string(&json_writer, propValue));

#define safe_add_json_property_object_begin(json_writer, propName) \
    _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&json_writer, AZ_SPAN_FROM_STR(propName))); \
    _az_RETURN_IF_FAILED(az_json_writer_append_begin_object(&json_writer));

static az_span split_az_span(az_span span, int32_t size, az_span* remainder);

AZ_NODISCARD bool az_iot_adu_ota_is_component_device_update(
    az_span component_name)
{
    return az_span_is_content_equal(
        AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_COMPONENT_NAME), component_name);
}

AZ_NODISCARD az_result az_iot_adu_ota_get_properties_payload(
    az_iot_hub_client const* iot_hub_client,
    az_iot_adu_ota_device_information* device_information,
    int32_t agent_state,
    az_iot_adu_ota_workflow* workflow,
    az_iot_adu_ota_install_result* last_install_result,
    az_span payload,
    az_span* out_payload)
{
    _az_PRECONDITION_NOT_NULL(iot_hub_client);
    _az_PRECONDITION_NOT_NULL(device_information);
    _az_PRECONDITION_VALID_SPAN(device_information->manufacturer, 1, false);
    _az_PRECONDITION_VALID_SPAN(device_information->model, 1, false);
    _az_PRECONDITION_VALID_SPAN(device_information->last_installed_update_id, 1, false);
    _az_PRECONDITION_VALID_SPAN(device_information->adu_version, 1, false);
    _az_PRECONDITION_VALID_SPAN(payload, 1, false);
    _az_PRECONDITION_NOT_NULL(out_payload);

    if (last_install_result != NULL)
    {
        _az_PRECONDITION_NOT_NULL(last_install_result->step_results);
        _az_PRECONDITION_RANGE(1, last_install_result->step_results_count, INT32_MAX);
    }

    az_json_writer jw;

    /* Update reported property */
    _az_RETURN_IF_FAILED(az_json_writer_init(&jw, payload, NULL));
    _az_RETURN_IF_FAILED(az_json_writer_append_begin_object(&jw));

    /* Fill the ADU agent component name.  */
    _az_RETURN_IF_FAILED(az_iot_hub_client_properties_writer_begin_component(
          iot_hub_client, &jw, AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_COMPONENT_NAME)));

    /* Fill the agent property name.  */
    safe_add_json_property_object_begin(jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_AGENT);

    /* Fill the deviceProperties.  */
    safe_add_json_property_object_begin(jw, 
        AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_DEVICEPROPERTIES);

    safe_add_json_property_az_span(jw,
        AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_MANUFACTURER,
        device_information->manufacturer);
    safe_add_json_property_az_span(jw,
        AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_MODEL,
        device_information->model);

    // TODO: verify if this needs to be exposed as an option.
    safe_add_json_property_string(jw,
        AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_INTERFACE_ID,
        AZ_IOT_ADU_OTA_AGENT_INTERFACE_ID);
    safe_add_json_property_az_span(jw,
        AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ADU_VERSION,
        device_information->adu_version);

    if (!az_span_is_content_equal(device_information->do_version, AZ_SPAN_EMPTY))
    {
        // TODO: verify if 'doVer' is required.
        //       Ref: https://docs.microsoft.com/en-us/azure/iot-hub-device-update/device-update-plug-and-play
        safe_add_json_property_az_span(jw,
            AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_DO_VERSION,
            device_information->do_version);
    }

    _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));

    /* Fill the compatible property names. */
    safe_add_json_property_string(jw,
        AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_COMPAT_PROPERTY_NAMES,
        AZ_IOT_ADU_OTA_AGENT_COMPATIBILITY);

    /* Add last installed update information */
    if (last_install_result != NULL)
    {
        safe_add_json_property_object_begin(jw, 
            AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_LAST_INSTALL_RESULT);

        safe_add_json_property_int32(
            jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RESULT_CODE,
            last_install_result->result_code);
        safe_add_json_property_int32(
            jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_EXTENDED_RESULT_CODE,
            last_install_result->extended_result_code);

        if (!az_span_is_content_equal(last_install_result->result_details, AZ_SPAN_EMPTY))
        {
            // TODO: Add quotes if result_details is not enclosed by quotes.
            safe_add_json_property_az_span(
                jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RESULT_DETAILS,
                last_install_result->result_details);
        }

        for (int32_t i = 0; i < last_install_result->step_results_count; i++)
        {
            _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&jw,
                last_install_result->step_results[i].step_id));
            _az_RETURN_IF_FAILED(az_json_writer_append_begin_object(&jw));

            safe_add_json_property_int32(
                jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RESULT_CODE,
                last_install_result->result_code);
            safe_add_json_property_int32(
                jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_EXTENDED_RESULT_CODE,
                last_install_result->extended_result_code);

            if (!az_span_is_content_equal(last_install_result->result_details, AZ_SPAN_EMPTY))
            {
                // TODO: Add quotes if result_details is not enclosed by quotes.
                safe_add_json_property_az_span(
                    jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RESULT_DETAILS,
                    last_install_result->result_details);
            }

            _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));
        }

        _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));
    }

    /* Fill the agent state.   */
    safe_add_json_property_int32(jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_STATE, agent_state);

    /* Fill the workflow.  */
    if (workflow != NULL)
    {
        safe_add_json_property_object_begin(jw, 
            AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_WORKFLOW);

        safe_add_json_property_int32(
            jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ACTION,
            workflow->action);
        safe_add_json_property_az_span(
            jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ID,
            trim_null_terminator(workflow->id));

        /* Append retry timestamp in workflow if existed.  */
        if (!az_span_is_content_equal(workflow->retry_timestamp, AZ_SPAN_EMPTY))
        {
            safe_add_json_property_az_span(
                jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RETRY_TIMESTAMP,
                trim_null_terminator(workflow->retry_timestamp));
        }
        _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));
    }

    /* Fill installed update id.  */
    // TODO: move last_installed_update_id out of this device_information structure.
    // TODO: rename device_information var and struct to device_properties to match json prop name.
    safe_add_json_property_az_span(
        jw, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_INSTALLED_CONTENT_ID,
        device_information->last_installed_update_id);

    _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));

    _az_RETURN_IF_FAILED(
        az_iot_hub_client_properties_writer_end_component(iot_hub_client, &jw));
    _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));

    *out_payload = az_json_writer_get_bytes_used_in_destination(&jw);

    return AZ_OK;
}

// Reference: AzureRTOS/AZ_IOT_ADU_OTA_agent_service_properties_get(...)
AZ_NODISCARD az_result az_iot_adu_ota_parse_service_properties(
    az_iot_hub_client const* iot_hub_client,
    az_json_reader* jr,
    az_span buffer,
    az_iot_adu_ota_update_request* update_request,
    az_span* buffer_remainder)
{
    _az_PRECONDITION_NOT_NULL(iot_hub_client);
    _az_PRECONDITION_NOT_NULL(jr);
    _az_PRECONDITION_VALID_SPAN(buffer, 1, false);
    _az_PRECONDITION_NOT_NULL(update_request);

    int32_t required_size;

    RETURN_IF_JSON_TOKEN_TYPE_NOT(jr, AZ_JSON_TOKEN_PROPERTY_NAME);
    RETURN_IF_JSON_TOKEN_TEXT_NOT(jr, AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_SERVICE);

    _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));
    RETURN_IF_JSON_TOKEN_TYPE_NOT(jr, AZ_JSON_TOKEN_BEGIN_OBJECT);
    _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));

    update_request->workflow.action = 0;
    update_request->workflow.id = AZ_SPAN_EMPTY;
    update_request->workflow.retry_timestamp = AZ_SPAN_EMPTY;
    update_request->update_manifest = AZ_SPAN_EMPTY;
    update_request->update_manifest_signature = AZ_SPAN_EMPTY;
    update_request->file_urls_count = 0;

    while (jr->token.kind != AZ_JSON_TOKEN_END_OBJECT)
    {
        RETURN_IF_JSON_TOKEN_TYPE_NOT(jr, AZ_JSON_TOKEN_PROPERTY_NAME);
        
        if (az_json_token_is_text_equal(&jr->token,
            AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_WORKFLOW)))
        {
            _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));
            RETURN_IF_JSON_TOKEN_TYPE_NOT(jr, AZ_JSON_TOKEN_BEGIN_OBJECT);
            _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));

            while (jr->token.kind != AZ_JSON_TOKEN_END_OBJECT)
            {
                RETURN_IF_JSON_TOKEN_TYPE_NOT(jr, AZ_JSON_TOKEN_PROPERTY_NAME);

                if (az_json_token_is_text_equal(&jr->token,
                    AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ACTION)))
                {
                    _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));
                    _az_RETURN_IF_FAILED(az_json_token_get_int32(&jr->token, &update_request->workflow.action));
                }
                else if (az_json_token_is_text_equal(&jr->token,
                    AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ID)))
                {
                    _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));

                    required_size = jr->token.size + NULL_TERM_CHAR_SIZE;

                    _az_RETURN_IF_NOT_ENOUGH_SIZE(buffer, required_size);

                    update_request->workflow.id = split_az_span(buffer, required_size, &buffer);

                    _az_RETURN_IF_FAILED(az_json_token_get_string(
                        &jr->token, (char*)az_span_ptr(update_request->workflow.id),
                        az_span_size(update_request->workflow.id), NULL));
                }
                else
                {
                    // TODO: log unexpected property.
                    return AZ_ERROR_JSON_INVALID_STATE;
                }

                _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));
            }
        }
        else if (az_json_token_is_text_equal(&jr->token,
            AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_UPDATE_MANIFEST)))
        {
            int32_t update_manifest_length;

            _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));

            _az_RETURN_IF_FAILED(az_json_token_get_string(
                &jr->token, (char*)az_span_ptr(buffer),
                az_span_size(buffer), &update_manifest_length));

            update_request->update_manifest
                = split_az_span(buffer, update_manifest_length, &buffer);
        }
        else if (az_json_token_is_text_equal(&jr->token,
            AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_UPDATE_MANIFEST_SIGNATURE)))
        {
            _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));

            required_size = jr->token.size + NULL_TERM_CHAR_SIZE;

            _az_RETURN_IF_NOT_ENOUGH_SIZE(buffer, required_size);

            update_request->update_manifest_signature = split_az_span(buffer, required_size, &buffer);

            _az_RETURN_IF_FAILED(az_json_token_get_string(
                &jr->token, (char*)az_span_ptr(update_request->update_manifest_signature),
                az_span_size(update_request->update_manifest_signature), NULL));
        }
        else if (az_json_token_is_text_equal(&jr->token,
            AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_FILEURLS)))
        {
            _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));
            RETURN_IF_JSON_TOKEN_TYPE_NOT(jr, AZ_JSON_TOKEN_BEGIN_OBJECT);
            _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));

            while (jr->token.kind != AZ_JSON_TOKEN_END_OBJECT)
            {
                RETURN_IF_JSON_TOKEN_TYPE_NOT(jr, AZ_JSON_TOKEN_PROPERTY_NAME);

                required_size = jr->token.size + NULL_TERM_CHAR_SIZE;

                _az_RETURN_IF_NOT_ENOUGH_SIZE(buffer, required_size);

                update_request->file_urls[update_request->file_urls_count].id = split_az_span(buffer, required_size, &buffer);

                _az_RETURN_IF_FAILED(az_json_token_get_string(
                    &jr->token, (char*)az_span_ptr(update_request->file_urls[update_request->file_urls_count].id),
                    az_span_size(update_request->file_urls[update_request->file_urls_count].id), NULL));

                _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));
                RETURN_IF_JSON_TOKEN_TYPE_NOT(jr, AZ_JSON_TOKEN_STRING);

                required_size = jr->token.size + NULL_TERM_CHAR_SIZE;

                _az_RETURN_IF_NOT_ENOUGH_SIZE(buffer, required_size);

                update_request->file_urls[update_request->file_urls_count].url = split_az_span(buffer, required_size, &buffer);

                _az_RETURN_IF_FAILED(az_json_token_get_string(
                    &jr->token, (char*)az_span_ptr(update_request->file_urls[update_request->file_urls_count].url),
                    az_span_size(update_request->file_urls[update_request->file_urls_count].url), NULL));

                update_request->file_urls_count++;

                _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));
            }
        }

        _az_RETURN_IF_FAILED(az_json_reader_next_token(jr));
    }

    if (buffer_remainder != NULL)
    {
        *buffer_remainder = buffer;
    }

    return AZ_OK;
}

AZ_NODISCARD az_result az_iot_adu_ota_get_service_properties_response(
    az_iot_hub_client const* iot_hub_client,
    az_iot_adu_ota_update_request* update_request,
    int32_t version,
    int32_t status,
    az_span payload,
    az_span* out_payload)
{
    _az_PRECONDITION_NOT_NULL(iot_hub_client);
    _az_PRECONDITION_NOT_NULL(update_request);
    _az_PRECONDITION_VALID_SPAN(payload, 1, false);
    _az_PRECONDITION_NOT_NULL(out_payload);

    az_json_writer jw;
    
    // Component and response status
    _az_RETURN_IF_FAILED(az_json_writer_init(&jw, payload, NULL));
    _az_RETURN_IF_FAILED(az_json_writer_append_begin_object(&jw));
    _az_RETURN_IF_FAILED(az_iot_hub_client_properties_writer_begin_component(
          iot_hub_client, &jw, AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_COMPONENT_NAME)));
    _az_RETURN_IF_FAILED(az_iot_hub_client_properties_writer_begin_response_status(
          iot_hub_client, &jw,
          AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_SERVICE), status, version, AZ_SPAN_EMPTY));

    _az_RETURN_IF_FAILED(az_json_writer_append_begin_object(&jw));

    // Workflow
    _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_WORKFLOW)));
    _az_RETURN_IF_FAILED(az_json_writer_append_begin_object(&jw));
    _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ACTION)));
    _az_RETURN_IF_FAILED(az_json_writer_append_int32(&jw, update_request->workflow.action));
    _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_ID)));
    _az_RETURN_IF_FAILED(az_json_writer_append_string(&jw, trim_null_terminator(update_request->workflow.id)));
    if (!az_span_is_content_equal(update_request->workflow.retry_timestamp, AZ_SPAN_EMPTY))
    {
        _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_RETRY_TIMESTAMP)));
        _az_RETURN_IF_FAILED(az_json_writer_append_string(&jw, trim_null_terminator(update_request->workflow.retry_timestamp)));
    }
    _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));

    // updateManifest
    _az_RETURN_IF_FAILED(az_json_writer_append_property_name(&jw, AZ_SPAN_FROM_STR(AZ_IOT_ADU_OTA_AGENT_PROPERTY_NAME_UPDATE_MANIFEST)));
    _az_RETURN_IF_FAILED(az_json_writer_append_string(&jw, trim_null_terminator(update_request->update_manifest)));

    _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));

    _az_RETURN_IF_FAILED(az_iot_hub_client_properties_writer_end_response_status(
        iot_hub_client, &jw));
    _az_RETURN_IF_FAILED(az_iot_hub_client_properties_writer_end_component(
          iot_hub_client, &jw));
    _az_RETURN_IF_FAILED(az_json_writer_append_end_object(&jw));

    *out_payload = az_json_writer_get_bytes_used_in_destination(&jw);

    return AZ_OK;
}


/* --- az_core extensions --- */
static az_span split_az_span(az_span span, int32_t size, az_span* remainder)
{
  az_span result = az_span_slice(span, 0, size);

  if (remainder != NULL)
  {
    if (az_span_is_content_equal(AZ_SPAN_EMPTY, result))
    {
        *remainder = AZ_SPAN_EMPTY;
    }
    else 
    {
        *remainder = az_span_slice(span, size, az_span_size(span));
    }
  }

  return result;
}