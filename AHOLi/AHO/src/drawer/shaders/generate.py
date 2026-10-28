"""標準TriangleのSPIR-Vを埋め込む。通常のビルドでは実行不要。"""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--glslc", default="glslc")
    parser.add_argument("--clang-format", default="clang-format")
    args = parser.parse_args()
    source = Path(__file__).resolve().parent
    lines = ["// shaders/generate.pyで生成。変更は元のGLSLへ反映する。",
             "#pragma once", "#include <cstdint>", "",
             "namespace AHO_NAMESPACE::drawer::detail {"]
    with tempfile.TemporaryDirectory() as temporary:
        for stage in ("vert", "frag"):
            output = Path(temporary) / (stage + ".spv")
            subprocess.run([args.glslc, "--target-env=vulkan1.0", "-O",
                            str(source / ("triangle." + stage)), "-o", str(output)], check=True)
            data = output.read_bytes()
            words = struct.unpack("<" + "I" * (len(data) // 4), data)
            lines.append(f"inline constexpr std::uint32_t TRIANGLE_{stage.upper()}[] = {{")
            for i in range(0, len(words), 8):
                lines.append("    " + ", ".join(f"0x{word:08x}" for word in words[i:i + 8]) + ",")
            lines.append("};")
    lines.append("} // namespace AHO_NAMESPACE::drawer::detail\n")
    header = source.parent / "standard_shaders.hpp"
    header.write_text("\n".join(lines), encoding="utf-8")
    subprocess.run([args.clang_format, "-i", str(header)], check=True)


if __name__ == "__main__":
    main()
