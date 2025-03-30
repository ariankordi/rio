from structs import Model
from gltf_reader import read_gltf
from packer import pack
import sys

def main():
    # Parse command line arguments
    bone_index_hack = False
    args = sys.argv[1:]

    if '-hack' in args:
        bone_index_hack = True
        args.remove('-hack')

    # The last argument is always the output file
    output_file = args[-1]
    input_files = args[:-1]

    # Create a model instance
    model = Model()
    model.meshes = []
    model.materials = []

    # Read each input file and append meshes to the model
    for input_file in input_files:
        meshes = read_gltf(input_file, bone_index_hack)
        if isinstance(meshes, list):
            model.meshes.extend(meshes)
        else:
            model.meshes.append(meshes)

    # Pack the model data
    dataLE = pack(model, '<')

    # Write the packed data to the output file
    with open(output_file, "wb") as outf:
        outf.write(dataLE)

if __name__ == "__main__":
    main()
