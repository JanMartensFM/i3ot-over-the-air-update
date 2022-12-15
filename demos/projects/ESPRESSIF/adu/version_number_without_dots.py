import os
import sys


# First argument should be the version number
if len(sys.argv) == 1:
    print ("The version number to use was not passed on the command line.")
    exit()

version_number = sys.argv[1]
version_number_without_dots = version_number.replace('.', '-')

env_file = os.getenv('GITHUB_ENV')

with open(env_file, "a") as myfile:
    myfile.write(f"VERSION_NUMBER_WITHOUT_DOTS={version_number_without_dots}")

print (f"Settting version number to {version_number_without_dots}.")


