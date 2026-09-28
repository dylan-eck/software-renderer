import sys


def main():
    if len(sys.argv) != 2:
        print("Usage: python gen_teapot_str.py <obj_file_path>")
        sys.exit(1)

    filename = sys.argv[1]

    with open(filename, 'r') as f:
        lines = f.readlines()

    modified_lines = []
    for line in lines:
        stripped = line.strip()
        # Drop comments and object names (e.g. Blender's export header)
        if stripped.startswith('#') or stripped.startswith('o '):
            continue
        if stripped.startswith(('v ', 'vn ', 'vt ')):
            parts = stripped.split()
            prefix = parts[0]
            try:
                floats = [float(x) for x in parts[1:]]
                formatted_floats = ['{:.2g}'.format(x) for x in floats]
                new_stripped = ' '.join([prefix] + formatted_floats)
                # Preserve original leading whitespace and trailing newline
                leading_whitespace = line[:line.find(prefix)]
                new_line = leading_whitespace + new_stripped + \
                    '\n' if line.endswith(
                        '\n') else leading_whitespace + new_stripped
            except ValueError:
                # If parsing fails, keep original line
                new_line = line
        else:
            new_line = line
        modified_lines.append(new_line)

    content = ''.join(modified_lines)
    escaped = content.replace('\\', '\\\\').replace('"', '\\"')
    print('"' + escaped.replace('\n', '\\n') + '"')


if __name__ == "__main__":
    main()
