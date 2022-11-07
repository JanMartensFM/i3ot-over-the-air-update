





# import os, uuid
# from azure.identity import DefaultAzureCredential, AzureCliCredential 
# from azure.storage.blob import BlobServiceClient, BlobClient, ContainerClient

# try:
#     print("Azure Blob Storage Python quickstart sample")

#     account_url = "https://rgtestjanstorageacc.blob.core.windows.net"

#     # Does not seem to work with DefaultAzureCredential.
#     # Using AzureCliCredential at the moment. Make sure to run "az login" before triggering.
#     # default_credential = DefaultAzureCredential()
#     default_credential = AzureCliCredential()

#     # Create the BlobServiceClient object
#     blob_service_client = BlobServiceClient(account_url, credential=default_credential)



#     # Retrieve the connection string for use with the application. The storage
#     # connection string is stored in an environment variable on the machine
#     # running the application called AZURE_STORAGE_CONNECTION_STRING. If the environment variable is
#     # created after the application is launched in a console or with Visual Studio,
#     # the shell or application needs to be closed and reloaded to take the
#     # environment variable into account.
#     # connect_str = "DefaultEndpointsProtocol=https;AccountName=rgtestjanstorageacc;AccountKey=uJyh90Uwi7JpzD9D62fNZObWB+n4ZTvh0sssbSQBfZ/zZQmjNtKi7QcS2BjGgQ9k5lk55NprzL4e+AStkvzF5g==;EndpointSuffix=core.windows.net"

#     # # Create the BlobServiceClient object
#     # blob_service_client = BlobServiceClient.from_connection_string(connect_str)


#     # Quickstart code goes here


#     # # Create a unique name for the container
#     # container_name = str(uuid.uuid4())

#     # # Create the container
#     # container_client = blob_service_client.create_container(container_name)

#     print("\nListing containers...")

#     # List the blobs in the container
#     container_list = blob_service_client.list_containers()
#     for container in container_list:
#         print("\t" + container.name)

#     print("\nListing blobs...")

#     container_client = blob_service_client.get_container_client(container="iotkitadu1-2")

#     # List the blobs in the container
#     blob_list = container_client.list_blobs()
#     for blob in blob_list:
#         print("\t" + blob.name)

# except Exception as ex:
#     print('Exception:')
#     print(ex)














# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License. See License.txt in the project root for
# license information.
# --------------------------------------------------------------------------

import os
import base64
import hashlib
import json

from azure.iot.deviceupdate import DeviceUpdateClient
from azure.identity import DefaultAzureCredential, AzureCliCredential
from azure.core.exceptions import HttpResponseError



def get_file_size(file_path):
    return os.path.getsize(file_path)


def get_file_hash(file_path):
    with open(file_path, "rb") as f:
        bytes = f.read()  # read entire file as bytes
        return base64.b64encode(hashlib.sha256(bytes).digest()).decode("utf-8")



def import_update(client, manifest_url, payload_url):
    print ("Importing update into IoT hub...")

    content = [{
        "importManifest": {
            "url": manifest_url,
            "sizeInBytes": get_file_size(manifest_file),
            "hashes": {
                "sha256": get_file_hash(manifest_file)
            }
        },
        "files": [{
            "fileName": os.path.basename(payload_file),
            "url": payload_url
        }]
    }]

    print (content)

    response = client.device_update.begin_import_update(content)
    response.wait

    result = response.result()

    print(result)

    return result

def deploy_update(client):
        print ("Triggering deployment to devices...")

        # {'updateId': {'provider': 'ESPRESSIF', 'name': 'ESP32-Azure-IoT-Kit', 'version': '1.3'}, 'isDeployable': True, 'compatibility': [{'deviceManufacturer': 'ESPRESSIF', 'deviceModel': 'ESP32-Azure-IoT-Kit'}], 'instructions': {'steps': [{'handler': 'microsoft/swupdate:1', 'files': ['azure_iot_freertos_esp32-v1.3.bin'], 'handlerProperties': {'installedCriteria': '1.3'}}]}, 'scanResult': 'Success', 'manifestVersion': '4.0', 'importedDateTime': '2022-11-03T15:40:22.4133178Z', 'createdDateTime': '2022-11-02T10:15:26.7924048Z', 'etag': '"d07fa4bf-0e5a-4fe2-b49a-2435437d4ae6"'}

        deployment = {
            'deploymentid': 'newdeployment',
            'groupid': 'azureiotkit-attempt2',
            'startDateTime': '2020-02-20 00:00:00',
            'update': 
                {'updateId': {'provider': 'ESPRESSIF', 'name': 'ESP32-Azure-IoT-Kit', 'version': '1.3'}}
            }

        result = client.device_management.create_or_update_deployment(group_id="azureiotkit-attempt2", deployment_id="newdeployment", deployment=deployment)
        # response = client.device_management.create_or_update_deployment(deployment=deployment)

        print (result)





# Set the values of the client ID, tenant ID, and client secret of the AAD application as environment variables:
# AZURE_CLIENT_ID, AZURE_TENANT_ID, AZURE_CLIENT_SECRET

# Set the following environment variables for this particular sample:
# DEVICEUPDATE_ENDPOINT, DEVICEUPDATE_INSTANCE_ID,
# DEVICEUPDATE_PAYLOAD_FILE, DEVICEUPDATE_PAYLOAD_URL, DEVICEUPDATE_MANIFEST_FILE, DEVICEUPDATE_MANIFEST_URL
try:

    # AZURE_CLIENT_ID: AAD service principal client id

    # AZURE_TENANT_ID: AAD tenant id

    # AZURE_CLIENT_SECRET or AZURE_CLIENT_CERTIFICATE_PATH: AAD service principal client secret


    endpoint = "fmaduseviceaccount.api.adu.microsoft.com"
    instance = "fmdevupdatetoiothub"
    
    update_provider = "ESPRESSIF"
    update_name = "ESP32-Azure-IoT-Kit"
    update_version = "1.3"
    
    # payload_file = os.environ["DEVICEUPDATE_PAYLOAD_FILE"]
    # payload_url = os.environ["DEVICEUPDATE_PAYLOAD_URL"]
    # manifest_file = os.environ["DEVICEUPDATE_MANIFEST_FILE"]
    # manifest_url = os.environ["DEVICEUPDATE_MANIFEST_URL"]

    
    # endpoint = os.environ["DEVICEUPDATE_ENDPOINT"]
    # instance = os.environ["DEVICEUPDATE_INSTANCE_ID"]
    # payload_file = os.environ["DEVICEUPDATE_PAYLOAD_FILE"]
    # payload_url = os.environ["DEVICEUPDATE_PAYLOAD_URL"]
    # manifest_file = os.environ["DEVICEUPDATE_MANIFEST_FILE"]
    # manifest_url = os.environ["DEVICEUPDATE_MANIFEST_URL"]

    # update_provider = os.environ["DEVICEUPDATE_UPDATE_PROVIDER"]
    # update_name = os.environ["DEVICEUPDATE_UPDATE_NAME"]
    # update_version = os.environ["DEVICEUPDATE_UPDATE_VERSION"]

    #TODO: use python library to specify path.
    payload_file = "C:/ADU-update/azure_iot_freertos_esp32-v1.3.bin"
    payload_url = "https://rgtestjanstorageacc.blob.core.windows.net/iotkitadu1-3/azure_iot_freertos_esp32-v1.3.bin"
    manifest_file = "C:/ADU-update/ESPRESSIF.ESP32-Azure-IoT-Kit.1.3.importmanifest.json"
    manifest_url = "https://rgtestjanstorageacc.blob.core.windows.net/iotkitadu1-3/ESPRESSIF.ESP32-Azure-IoT-Kit.1.3.importmanifest.json"



except KeyError:
    print("Missing one of environment variables: DEVICEUPDATE_ENDPOINT, DEVICEUPDATE_INSTANCE_ID, "
          "DEVICEUPDATE_PAYLOAD_FILE, DEVICEUPDATE_PAYLOAD_URL, DEVICEUPDATE_MANIFEST_FILE, DEVICEUPDATE_MANIFEST_URL")
    exit()


# =============================

default_credential = AzureCliCredential()

# Build a client through AAD
# client = DeviceUpdateClient(credential=DefaultAzureCredential(), endpoint=endpoint, instance_id=instance)
client = DeviceUpdateClient(credential=default_credential, endpoint=endpoint, instance_id=instance)

try:
    # print("List providers, names and versions of updates in Device Update for IoT Hub...")

    # print("\nProviders:")
    # response = client.device_update.list_providers()
    # for item in response:
    #     print(f"  {item}")

    # print(f"\nNames in '{update_provider}'")
    # response = client.device_update.list_names(update_provider)
    # for item in response:
    #     print(f"  {item}")

    # print(f"\nVersions in provider '{update_provider}' and name '{update_name}'")
    # response = client.device_update.list_versions(update_provider, update_name)
    # for item in response:
    #     print(f"  {item}")


    # TODO
    # 0. Build FreeRTOS image, set version numbers, create manifest file.
    # 1. Create Container
    # 2. Set security correct
    # 3. Upload files
    # 4. Use these files in specifying the content.

    # 6. Trigger deployment
    # 7. (Check if there are old deployments that need to be disabled ?)


    # Deploying new version.

    print ("Importing update into IoT hub...")

    content = [{
        "importManifest": {
            "url": manifest_url,
            "sizeInBytes": get_file_size(manifest_file),
            "hashes": {
                "sha256": get_file_hash(manifest_file)
            }
        },
        "files": [{
            "fileName": os.path.basename(payload_file),
            "url": payload_url
        }]
    }]

    print (content)

    response = client.device_update.begin_import_update(content)
    response.wait

    result = response.result()

    print(result)

    # If import succeeded, deploy.
    if True:

        print ("Triggering deployment to devices...")

        # {'updateId': {'provider': 'ESPRESSIF', 'name': 'ESP32-Azure-IoT-Kit', 'version': '1.3'}, 'isDeployable': True, 'compatibility': [{'deviceManufacturer': 'ESPRESSIF', 'deviceModel': 'ESP32-Azure-IoT-Kit'}], 'instructions': {'steps': [{'handler': 'microsoft/swupdate:1', 'files': ['azure_iot_freertos_esp32-v1.3.bin'], 'handlerProperties': {'installedCriteria': '1.3'}}]}, 'scanResult': 'Success', 'manifestVersion': '4.0', 'importedDateTime': '2022-11-03T15:40:22.4133178Z', 'createdDateTime': '2022-11-02T10:15:26.7924048Z', 'etag': '"d07fa4bf-0e5a-4fe2-b49a-2435437d4ae6"'}

        deployment = {
            'deploymentid': 'newdeployment',
            'groupid': 'azureiotkit-attempt2',
            'startDateTime': '2020-02-20 00:00:00',
            'update': 
                {'updateId': {'provider': 'ESPRESSIF', 'name': 'ESP32-Azure-IoT-Kit', 'version': '1.3'}}
            }

        result = client.device_management.create_or_update_deployment(group_id="azureiotkit-attempt2", deployment_id="newdeployment", deployment=deployment)
        # response = client.device_management.create_or_update_deployment(deployment=deployment)

        print (result)
    
    print ("Finished")

except HttpResponseError as e:
    print('Failed to import update: {}'.format(e))

