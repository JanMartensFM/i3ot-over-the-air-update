import os
import sys
import base64
import hashlib

def get_file_size(file_path):
    return os.path.getsize(file_path)

def get_file_hash(file_path):
    with open(file_path, "rb") as f:
        bytes = f.read()  # read entire file as bytes
        return base64.b64encode(hashlib.sha256(bytes).digest()).decode("utf-8")


def update_manifest(firmwarefile, manifest_file_name, manifest_template_filename, version_number):
    manifest_template = open(manifest_template_filename, "r")

    manifest_template_contents = manifest_template.read()

    manifest_template.close()

    file_size = get_file_size(firmwarefile)
    file_hash = get_file_hash(firmwarefile)

    print (file_size)
    print (file_hash)


    manifest_template_contents = manifest_template_contents.replace("{{sizeInBytes}}", str(file_size))
    manifest_template_contents = manifest_template_contents.replace("{{hashCode}}", str(file_hash))
    manifest_template_contents = manifest_template_contents.replace("{{versionNumber}}", version_number)

    print (manifest_template_contents)

    updated_manifest_file = open(manifest_file_name, "w")

    updated_manifest_file.write(manifest_template_contents)
    updated_manifest_file.close()


# Parameters for this deployment
firmware_file = "demos/projects/ESPRESSIF/adu/azure_iot_freertos_esp32.bin"
manifest_file = "demos/projects/ESPRESSIF/adu/ESPRESSIF.ESP32-Azure-IoT-Kit.importmanifest.json"
manifest_template = "demos/projects/ESPRESSIF/adu/ESPRESSIF.ESP32-Azure-IoT-Kit.importmanifest.template.json"

# Use this for debugging.
# firmware_file = "ESPRESSIF.ESP32-Azure-IoT-Kit.importmanifest.template.json"
# manifest_file = "ESPRESSIF.ESP32-Azure-IoT-Kit.importmanifest.json"
# manifest_template = "ESPRESSIF.ESP32-Azure-IoT-Kit.importmanifest.template.json"



# First argument should be the version number
if len(sys.argv) == 1:
    print ("The version number to use was not passed on the command line.")
    exit()

version_number = sys.argv[1]

print (f"Settting version number to {version_number}.")

update_manifest(firmware_file, manifest_file, manifest_template, version_number)
