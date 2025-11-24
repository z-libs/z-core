import os
import re
import sys
import argparse

COMMON_FILENAME = "zcommon.h"

COMMON_GUARD = "Z_COMMON_BUNDLED"

def get_file_content(filepath):
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        sys.exit(1)
    with open(filepath, 'r', encoding='utf-8') as f:
        return f.read()

def strip_local_include(content, header_name):
    """
    Removes lines like #include "zcommon.h" from the source.
    It matches quotes specifically, leaving <stdlib.h> alone.
    """
    pattern = re.compile(fr'^\s*#include\s*"{re.escape(header_name)}".*$', re.MULTILINE)
    return pattern.sub(f'// [Bundled] "{header_name}" is included inline in this same file', content)

def create_bundle(source_path, common_path, output_path):
    print(f"Bundling:\n  Source: {source_path}\n  Common: {common_path}\n  Output: {output_path}")

    source_code = get_file_content(source_path)
    common_code = get_file_content(common_path)

    common_payload = f"""
/* ============================================================================
   z-libs Common Definitions (Bundled)
   This block is auto-generated. It is guarded so that if you include multiple
   z-libs it is only defined once.
   ============================================================================ */
#ifndef {COMMON_GUARD}
#define {COMMON_GUARD}

{common_code}

#endif // {COMMON_GUARD}
/* ============================================================================ */
"""

    processed_source = strip_local_include(source_code, COMMON_FILENAME)

    output_dir = os.path.dirname(output_path)
    if output_dir:  # Only try to create dirs if there IS a directory path
        os.makedirs(output_dir, exist_ok=True)
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("/*\n")
        f.write(" * GENERATED FILE - DO NOT EDIT DIRECTLY\n")
        f.write(f" * Source: {os.path.basename(source_path)}\n")
        f.write(" *\n")
        f.write(" * This file is part of the z-libs collection: https://github.com/z-libs\n")
        f.write(" * Licensed under the MIT License.\n")
        f.write(" */\n\n")
        
        f.write(common_payload)
        f.write("\n")
        f.write(processed_source)

    print(f"Success! Created {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Bundle a z-lib header with zcommon.h")
    parser.add_argument("source", help="Path to the raw library file (e.g., src/zvec_impl.h)")
    parser.add_argument("output", help="Path to the destination file (e.g., dist/zvec.h)")
    
    default_common = os.path.join(os.path.dirname(os.path.abspath(__file__)), COMMON_FILENAME)
    parser.add_argument("--common", default=default_common, help="Path to zcommon.h")

    args = parser.parse_args()
    
    create_bundle(args.source, args.common, args.output)
