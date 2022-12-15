import os
import sys


def update_demo_config(demo_config_file_name, version_number):
    demo_config_file = open(demo_config_file_name, "r")

    demo_config_contents = demo_config_file.read()

    demo_config_file.close()


    start_version_section = demo_config_contents.find("/*START-democonfigADU_UPDATE_VERSION*/")

    end_version_section = demo_config_contents.find("/*END-democonfigADU_UPDATE_VERSION*/")


    text_to_insert = f'/*START-democonfigADU_UPDATE_VERSION*/\n#define democonfigADU_UPDATE_VERSION      \"{version_number}\"\n'


    demo_config_contents = demo_config_contents[:start_version_section] + text_to_insert + demo_config_contents[end_version_section:]

    
    print (demo_config_contents)
    


    demo_config_file = open(demo_config_file_name, "w")

    demo_config_file.write(demo_config_contents)

    demo_config_file.close()

# Parameters for this deployment
demo_config_file = "demos/projects/ESPRESSIF/adu/config/demo_config.h"

# Use this for debugging.
# demo_config_file = "config/demo_config.h"

# First argument should be the version number
if len(sys.argv) == 1:
    print ("The version number to use was not passed on the command line.")
    exit()

version_number = sys.argv[1]

print (f"Settting version number to {version_number}.")

update_demo_config(demo_config_file, version_number)
