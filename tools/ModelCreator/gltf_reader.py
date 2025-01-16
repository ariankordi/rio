import struct
from pygltflib import GLTF2
from structs import Vertex
from structs import Mesh

def read_gltf(path, bone_index_hack=False):
    if bone_index_hack:
        print("bone index hack enabled, JOINTS_0 mapped to texcoord in final rmdl")
    meshes = []

    gltf = GLTF2().load(path)
    buffer = gltf.binary_blob()
    accessors = gltf.accessors
    bufferViews = gltf.bufferViews

    for gltf_mesh in gltf.meshes:
        primitive = gltf_mesh.primitives[0]

        # Reading indices
        index_buffer_index = primitive.indices
        index_accessor = accessors[index_buffer_index]
        index_count = index_accessor.count
        index_buffer_view = bufferViews[index_accessor.bufferView]
        indices = struct.unpack_from(f"<{index_count}H", buffer, index_buffer_view.byteOffset)

        # Reading positions
        position_buffer_index = primitive.attributes.POSITION
        position_accessor = accessors[position_buffer_index]
        position_count = position_accessor.count
        position_buffer_view = bufferViews[position_accessor.bufferView]
        positions = [struct.unpack_from("<3f", buffer, position_buffer_view.byteOffset + i * 12) for i in range(position_count)]

        # Reading normals
        normal_buffer_index = primitive.attributes.NORMAL
        normal_accessor = accessors[normal_buffer_index]
        normal_count = normal_accessor.count
        normal_buffer_view = bufferViews[normal_accessor.bufferView]
        normals = [struct.unpack_from("<3f", buffer, normal_buffer_view.byteOffset + i * 12) for i in range(normal_count)]

        if bone_index_hack:
            # HACK: Read JOINTS_0 as vec2 float (x mapped, y = 0)
            try:
                joints_buffer_index = primitive.attributes.JOINTS_0
                joints_accessor = accessors[joints_buffer_index]
                joints_count = joints_accessor.count
                joints_buffer_view = bufferViews[joints_accessor.bufferView]
                tex_coords = [(struct.unpack_from("<B", buffer, joints_buffer_view.byteOffset + i * 4)[0], 0.0)
                              for i in range(joints_count)]
            except (AttributeError, KeyError):
                # If JOINTS_0 is missing, default to zero values
                tex_coords = [(0.0, 0.0)] * position_count
        else:
            # Reading texture coordinates
            tex_coord_buffer_index = primitive.attributes.TEXCOORD_0
            tex_coord_accessor = accessors[tex_coord_buffer_index]
            tex_coord_count = tex_coord_accessor.count
            tex_coord_buffer_view = bufferViews[tex_coord_accessor.bufferView]
            tex_coords = [struct.unpack_from("<2f", buffer, tex_coord_buffer_view.byteOffset + i * 8) for i in range(tex_coord_count)]

        vertices = []
        for pos, tex_coord, normal in zip(positions, tex_coords, normals):
            vertices.append(Vertex(pos, tex_coord, normal))

        mesh = Mesh()
        mesh.vertices = vertices
        mesh.indices = list(indices)
        mesh.materialIdx = 0

        meshes.append(mesh)

    return meshes
