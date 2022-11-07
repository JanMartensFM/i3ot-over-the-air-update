import os
import base64
import hashlib
import json

from azure.iot.deviceupdate import DeviceUpdateClient
from azure.identity import DefaultAzureCredential, AzureCliCredential
from azure.core.exceptions import HttpResponseError
from azure.storage.blob import BlobServiceClient, BlobClient, ContainerClient


def format_for_azure(string_to_format):
    return string_to_format.lower().replace("_", "-").replace(".", "-")

def define_container_name(update_provider, update_name, update_version):
    return format_for_azure(update_provider) + '-' + format_for_azure(update_name) + '-' + format_for_azure(update_version)


def define_deployment_id(group_id, update_provider, update_name, update_version):
    return format_for_azure(group_id) + '-' + format_for_azure(update_provider) + '-' + format_for_azure(update_name) + '-' + format_for_azure(update_version)


def get_file_size(file_path):
    return os.path.getsize(file_path)

def get_file_hash(file_path):
    with open(file_path, "rb") as f:
        bytes = f.read()  # read entire file as bytes
        return base64.b64encode(hashlib.sha256(bytes).digest()).decode("utf-8")

def upload_filecontents(blob_client, file_completepath):
    with open(file=file_completepath, mode="rb") as data:
        blob_client.upload_blob(data, overwrite=True)
    

def upload_files(storage_account_url, credential, container_name, firmware_file, manifest_file):
    try:
        print ("Uploading files to storage container...")

        # Create the BlobServiceClient object
        blob_service_client = BlobServiceClient(account_url=storage_account_url, credential=credential)

        # Create the container if it does not exist.
        container_client = blob_service_client.get_container_client(container=container_name)
        if not container_client.exists():
            container_client.create_container(public_access='blob')
            # container_client = blob_service_client.create_container(name=container_name,public_access='blob')

        firmware_filename =  os.path.basename(firmware_file)
        firmware_blob_client = container_client.get_blob_client(firmware_filename)
        upload_filecontents(firmware_blob_client, firmware_file)

        manifest_filename =  os.path.basename(manifest_file)
        manifest_blob_client = container_client.get_blob_client(manifest_filename)
        upload_filecontents(manifest_blob_client, manifest_file)

        print ("Finished upload.")

        return firmware_blob_client.url, manifest_blob_client.url

    except Exception as ex:
        print('Exception:')
        print(ex)

def update_version_exists(client, update_provider, update_name, update_version):
    updateversions = client.device_update.list_versions(update_provider, update_name)

    for updateversion in updateversions:
        if updateversion == update_version:
            return True


    return False


def import_update(client, firmware_file, firmware_url, manifest_file, manifest_url, update_provider, update_name, update_version):

    # Check if the update already exists, and delete if so.
    # response = client.device_update.get_update(update_provider, update_name, update_version)
    if update_version_exists(client, update_provider, update_name, update_version):
        print ("Previous version of update found, deleting...")

        response = client.device_update.begin_delete_update(update_provider, update_name, update_version)
        response.wait
        result = response.result()

        print ("Previous version of update deleted.")

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
            "fileName": os.path.basename(firmware_file),
            "url": firmware_url
        }]
    }]

    response = client.device_update.begin_import_update(content)
    response.wait

    result = response.result()

    print ("Imported update into IoT hub.")

    return result

def deploy_update(client, deployment_id, group_id, starttime, update_provider, update_name, update_version):
        print ("Triggering deployment to devices...")

        # {'updateId': {'provider': 'ESPRESSIF', 'name': 'ESP32-Azure-IoT-Kit', 'version': '1.3'}, 'isDeployable': True, 'compatibility': [{'deviceManufacturer': 'ESPRESSIF', 'deviceModel': 'ESP32-Azure-IoT-Kit'}], 'instructions': {'steps': [{'handler': 'microsoft/swupdate:1', 'files': ['azure_iot_freertos_esp32-v1.3.bin'], 'handlerProperties': {'installedCriteria': '1.3'}}]}, 'scanResult': 'Success', 'manifestVersion': '4.0', 'importedDateTime': '2022-11-03T15:40:22.4133178Z', 'createdDateTime': '2022-11-02T10:15:26.7924048Z', 'etag': '"d07fa4bf-0e5a-4fe2-b49a-2435437d4ae6"'}

        deployment = {
            'deploymentid': deployment_id,
            'groupid': group_id,
            'startDateTime': starttime,
            'update': 
                {'updateId': {'provider': update_provider, 'name': update_name, 'version': update_version}}
            }

        result = client.device_management.create_or_update_deployment(group_id=group_id, deployment_id=deployment_id, deployment=deployment)
        print (result)


# Steps
# 0. Build FreeRTOS image, set version numbers, create manifest file.
# 1. Create Container
# 2. Set security correct
# 3. Upload files
# 4. Use these files in specifying the content.

# 6. Trigger deployment
# 7. (Check if there are old deployments that need to be disabled ?)

# TODO => check if import was succesful
# TODO => check if deployment needs to be cancelled


# Parameters for this deployment
firmware_file = "C:/ADU-update/azure_iot_freertos_esp32-v1.3.bin"
manifest_file = "C:/ADU-update/ESPRESSIF.ESP32-Azure-IoT-Kit.1.3.importmanifest.json"

update_provider = "ESPRESSIF"
update_name = "ESP32-Azure-IoT-Kit"
update_version = "1.3"

group_id = 'azureiotkit-attempt2'
start_time = "2020-02-20 00:00:00" # Deploy now

# parameters
storage_account_url = "https://rgtestjanstorageacc.blob.core.windows.net"
device_update_endpoint = "fmaduseviceaccount.api.adu.microsoft.com"
device_update_instance = "fmdevupdatetoiothub"

# Does not seem to work with DefaultAzureCredential.
# Using AzureCliCredential at the moment. Make sure to run "az login" before triggering.
# default_credential = DefaultAzureCredential()
credential = AzureCliCredential()

# Generate based on version to deploy
container_name = define_container_name(update_provider, update_name, update_version)
deployment_id = define_deployment_id(group_id, update_provider, update_name, update_version)

firmware_url, manifest_url =  upload_files(storage_account_url=storage_account_url, credential=credential, container_name=container_name, firmware_file=firmware_file, manifest_file=manifest_file)

client = DeviceUpdateClient(credential=credential, endpoint=device_update_endpoint, instance_id=device_update_instance)

import_update(client=client, firmware_file=firmware_file, firmware_url=firmware_url, manifest_file=manifest_file,manifest_url=manifest_url, update_provider=update_provider, update_name=update_name, update_version=update_version)

deploy_update(client=client, deployment_id=deployment_id, group_id=group_id, starttime=start_time, update_provider=update_provider, update_name=update_name, update_version=update_version)
