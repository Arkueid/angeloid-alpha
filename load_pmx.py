from pymeshio.pmx import reader


class PmxModel:
    def __init__(self, path: str):
        self._model = reader.read_from_file(path)

    @property
    def name(self) -> str:
        return self._model.name

    @property
    def english_name(self) -> str:
        return self._model.english_name

    @property
    def comment(self) -> str:
        return self._model.comment

    @property
    def vertices(self):
        return self._model.vertices

    @property
    def vertex_count(self) -> int:
        return len(self._model.vertices)

    @property
    def indices(self):
        return self._model.indices

    @property
    def face_count(self) -> int:
        return len(self._model.indices) // 3

    @property
    def textures(self):
        return self._model.textures

    @property
    def texture_count(self) -> int:
        return len(self._model.textures)

    @property
    def materials(self):
        return self._model.materials

    @property
    def material_count(self) -> int:
        return len(self._model.materials)

    @property
    def bones(self):
        return self._model.bones

    @property
    def bone_count(self) -> int:
        return len(self._model.bones)

    @property
    def morphs(self):
        return self._model.morphs

    @property
    def morph_count(self) -> int:
        return len(self._model.morphs)

    @property
    def display_slots(self):
        return self._model.display_slots

    def __repr__(self):
        return (f"PmxModel(name={self.name}, vertices={self.vertex_count}, "
                f"faces={self.face_count}, bones={self.bone_count})")


def main():
    model_path = "resources/ikaros-origin/Ikaros.pmx"

    print(f"Loading PMX model: {model_path}")
    model = PmxModel(model_path)

    print(f"\n=== Model Info ===")
    print(f"Name: {model.name}")
    print(f"English Name: {model.english_name}")
    print(f"Comment: {model.comment}")

    print(f"\n=== Vertices ===")
    print(f"Vertex count: {model.vertex_count}")
    if model.vertices:
        v = model.vertices[0]
        print(f"First vertex position: {v.position}")

    print(f"\n=== Indices ===")
    print(f"Triangle count: {model.face_count}")

    print(f"\n=== Textures ===")
    print(f"Texture count: {model.texture_count}")
    for i, tex in enumerate(model.textures):
        print(f"  [{i}] {tex}")

    print(f"\n=== Materials ===")
    print(f"Material count: {model.material_count}")
    for i, mat in enumerate(model.materials):
        print(f"  [{i}] {mat.name}")

    print(f"\n=== Bones ===")
    print(f"Bone count: {model.bone_count}")
    for i, bone in enumerate(model.bones[:10]):
        print(f"  [{i}] {bone.name}")
    if model.bone_count > 10:
        print(f"  ... and {model.bone_count - 10} more bones")

    print(f"\n=== Morphs ===")
    print(f"Morph count: {model.morph_count}")

    print(f"\n{model}")

if __name__ == "__main__":
    main()