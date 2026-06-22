# Mesh3D Test Assets — Attribution

These glTF sample models are used to test the DiaAssetPipeline glTF→`.mesh3d`
cooking handler against real exporter output (not hand-built fixtures).

Source: [KhronosGroup/glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets)

| File(s) | Model | License | Notes |
|---------|-------|---------|-------|
| `Avocado.gltf` + `Avocado.bin` | Avocado | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) (public domain) | PBR model — has POSITION/NORMAL/**TANGENT**/TEXCOORD_0, so it **cooks**. 406 verts, 2046 indices, uint16. External sidecar `.bin`. |
| `Box.gltf` + `Box0.bin` | Box | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) — © Cesium | Minimal cube — only POSITION/NORMAL (no tangent/UV), so it is **correctly rejected** by validation. Useful as a real-world reject case. |
| `Box.glb` | Box | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) — © Cesium | Same Box as a binary `.glb` (buffer in the GLB binary chunk, `uri = null`). Exercises the binary-blob read path. |

The Box models are © Cesium and licensed CC BY 4.0; attribution is provided here
per the license terms.
